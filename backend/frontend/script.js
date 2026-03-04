class MandelbrotViewer {
  constructor() {
    this.socketUrl = `ws://${window.location.host}/ws`;
    this.canvas = document.getElementById('mandelbrotCanvas');
    this.ctx = this.canvas.getContext('2d', { alpha: true }); // Optimize for no alpha

    this.state = {
      x: -2.3,
      y: -1.2,
      w: 4.0,
      h: 4.0,
      iter: 1000
    };
    this.initialState = {
      x: -2.3,
      y: -1.2,
      w: 4.0,
      h: 4.0,
      iter: 1000
    };

    this.isDragging = false;
    this.lastMouse = { x: 0, y: 0 };
    this.resolutionDivider = 1;
    this.animationPath = [];
    this.isPlaying = false;
    this.socket = null;
    this.pendingRequest = false;
    this.maxIterations = 10000;
    this.hasMoved = false;
    this.lastFrameTime = performance.now();

    this.currentTheme = 'ocean';
    this.palette = new Uint32Array(this.maxIterations + 1);
    this.generatePalette(this.currentTheme);
    this.init();
  }

  init() {
    this.connect();
    this.setupListeners();
    this.resize();
    window.addEventListener('resize', () => this.resize());
    this.renderLoop();
  }
  generatePalette(themeName) {
    this.currentTheme = themeName || 'ocean';

    for (let i = 0; i <= this.maxIterations; i++) {
      if (i === this.maxIterations) {
        this.palette[i] = 0xFF000000;
        continue;
      }

      let r, g, b;
      // Normalize iteration count. 
      // Multiplier '0.1' determines how fast the colors cycle (lower = wider bands)
      const t = i * 0.2;

      switch (this.currentTheme) {
        case 'fire':
          r = Math.floor((0.6 + 0.4 * Math.cos(t)) * 255);
          g = Math.floor((0.4 + 0.3 * Math.cos(t + 1.0)) * 255);
          b = Math.floor((0.2 + 0.2 * Math.cos(t + 2.0)) * 255);
          break;

        case 'matrix':
          r = 0;
          g = Math.floor((0.6 + 0.4 * Math.sin(t * 0.5)) * 255);
          b = 0;
          if (g < 60) g = 0;
          break;

        case 'rainbow':
          r = Math.floor((0.5 + 0.5 * Math.sin(t)) * 255);
          g = Math.floor((0.5 + 0.5 * Math.sin(t + 2.09)) * 255);
          b = Math.floor((0.5 + 0.5 * Math.sin(t + 4.18)) * 255);
          break;

        case 'zebra':
          const v = (i % 20) < 10 ? 255 : 0;
          r = g = b = v;
          break;

        case 'ocean':
        default:
          r = Math.floor(25 + 25 * Math.cos(t));
          g = Math.floor(100 + 80 * Math.sin(t * 0.5));
          b = Math.floor(200 + 55 * Math.cos(t));
          break;
      }

      // Pack into Little Endian ARGB Integer (0xAABBGGRR)
      this.palette[i] = (255 << 24) | (b << 16) | (g << 8) | r;
    }
  }
  connect() {
    this.socket = new WebSocket(this.socketUrl);
    this.socket.binaryType = 'arraybuffer';

    const statusDot = document.getElementById('statusDot');

    this.socket.onopen = () => {
      console.log("Connected to Backend");
      statusDot.classList.add('connected');
      statusDot.classList.remove('disconnected');
      this.requestFrame();
    };

    this.socket.onclose = () => {
      console.log("Disconnected");
      statusDot.classList.remove('connected');
      statusDot.classList.add('disconnected');
      setTimeout(() => this.connect(), 3000);
    };

    this.socket.onmessage = (event) => {
      this.handleResponse(event.data);
    };
  }

  updateAspect() {
    const aspect = this.canvas.width / this.canvas.height;
    // Keep x_scale fixed, adjust y_scale (height) to match aspect
    // This prevents stretching when window resizes
    this.state.h = this.state.w / aspect;
  }

  getParams() {
    const width = Math.floor(this.canvas.width / this.resolutionDivider);
    const height = Math.floor(this.canvas.height / this.resolutionDivider);

    const xScale = this.state.w / width;
    const yScale = this.state.h / height; // Should be roughly same as xScale

    return {
      x_min: this.state.x,
      y_min: this.state.y,
      x_scale: xScale,
      y_scale: yScale,
      width: width,
      height: height,
      iterations: this.state.iter
    };
  }


  requestFrame() {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) return;
    if (this.pendingRequest) return; // Flow control: don't flood server

    const p = this.getParams();
    this.requestedIter = p.iterations;
    // Protocol: "x_min,y_min,x_scale,y_scale,width,height,iterations"
    const msg = `${p.x_min.toFixed(15)},${p.y_min.toFixed(15)},${p.x_scale.toFixed(15)},${p.y_scale.toFixed(15)},${p.width},${p.height},${p.iterations}`;

    this.socket.send(msg);
    this.pendingRequest = true;
    if (this.loadingTimeout)
      clearTimeout(this.loadingTimeout);
    this.loadingTimeout = setTimeout(() => {
      if (this.pendingRequest) {
        document.getElementById('loading').classList.remove('hidden');
      }
    }, 300);
  }

  handleResponse(arrayBuffer) {
    if (this.loadingTimeout)
      clearTimeout(this.loadingTimeout);
    this.pendingRequest = false;
    document.getElementById('loading').classList.add('hidden');

    const width = Math.floor(this.canvas.width / this.resolutionDivider);
    const height = Math.floor(this.canvas.height / this.resolutionDivider);
    const expectedSize = width * height * 2; // 2 bytes per pixel (Uint16)

    // If the received buffer size doesn't match what we expect, 
    // it is likely a stale packet from a previous resolution or resize event.
    // We simply ignore it and wait for the next correct frame.
    if (arrayBuffer.byteLength !== expectedSize) {
      console.warn("Dropping stale frame: Size mismatch");
      this.requestFrame()
      return;
    }
    if (arrayBuffer.byteLength < width * height * 2) {
      console.error("Buffer too small");
      return;
    }

    // Clone the buffer so we can re-use it for theme changes
    // We use Uint16Array to interpret the raw bytes
    this.lastIterations = new Uint16Array(arrayBuffer);
    this.renderedIter = this.requestedIter;

    this.draw();
    this.updateUI();
    if (this.isPlaying) this.advanceAnimation();
  }

  draw() {
    if (!this.lastIterations) return;

    const width = Math.floor(this.canvas.width / this.resolutionDivider);
    const height = Math.floor(this.canvas.height / this.resolutionDivider);

    const imageData = new ImageData(width, height);
    const pixelView = new Uint32Array(imageData.data.buffer);

    // Map Iterations -> Colors using the CURRENT palette
    const len = width * height;
    const maxVal = this.renderedIter;
    for (let k = 0; k < len; k++) {
      const val = this.lastIterations[k];
      pixelView[k] = (val >= maxVal) ? 0xFF000000 : this.palette[val];
    }

    if (this.resolutionDivider > 1) {
      createImageBitmap(imageData).then(bitmap => {
        this.ctx.imageSmoothingEnabled = false;
        this.ctx.drawImage(bitmap, 0, 0, this.canvas.width, this.canvas.height);
      });
    } else {
      this.ctx.putImageData(imageData, 0, 0);
    }
  }
  setupListeners() {
    const themeSelect = document.getElementById('themeSelect');
    themeSelect.addEventListener('change', (e) => {
      this.generatePalette(e.target.value);
      this.draw();
    });
    this.canvas.addEventListener('dblclick', e => {
      e.preventDefault();

      // 1. Reset to initial values (from constructor)
      this.state = structuredClone(this.initialState);
      // 2. Recalculate height based on current window aspect ratio
      // (Crucial so the image doesn't look stretched if the window size changed)
      this.updateAspect();

      // 3. Render
      this.requestFrame();
    });
    this.canvas.addEventListener('mousedown', e => {
      this.isDragging = true;
      this.lastMouse = { x: e.clientX, y: e.clientY };
    });

    window.addEventListener('mouseup', () => {
      this.isDragging = false;
      if (this.socket.readyState === WebSocket.OPEN) this.requestFrame(); // Final high-res render
    });

    window.addEventListener('mousemove', e => {
      if (!this.isDragging) return;
      const dx = e.clientX - this.lastMouse.x;
      const dy = e.clientY - this.lastMouse.y;
      this.lastMouse = { x: e.clientX, y: e.clientY };

      const p = this.getParams();
      // Move opposite to drag
      this.state.x -= dx * p.x_scale / this.resolutionDivider;
      this.state.y -= dy * p.y_scale / this.resolutionDivider;
      this.hasMoved = true;

      // Throttle requests during drag could be added here, 
      // but for local AVX backend, usually fast enough to just request.
      // Using requestAnimationFrame to debounce visually
    });

    // Zoom
    this.canvas.addEventListener('wheel', e => {
      e.preventDefault();
      const zoomFactor = e.deltaY > 0 ? 1.1 : 0.9;

      const rect = this.canvas.getBoundingClientRect();

      // 1. Calculate normalized mouse position (0.0 to 1.0)
      // This decouples us from the internal resolution
      const u = (e.clientX - rect.left) / rect.width;
      const v = (e.clientY - rect.top) / rect.height;

      // 2. Calculate the World Point under the mouse *before* zoom
      const mathX = this.state.x + u * this.state.w;
      const mathY = this.state.y + v * this.state.h;

      // 3. Apply Zoom
      this.state.w *= zoomFactor;
      this.state.h *= zoomFactor;

      // 4. Calculate new Top-Left so that 'mathX' is still at position 'u'
      // mathX = new_x + u * new_w
      // new_x = mathX - u * new_w
      this.state.x = mathX - u * this.state.w;
      this.state.y = mathY - v * this.state.h;

      this.requestFrame();
    }, { passive: false });
    // UI Controls
    document.getElementById('iterSlider').addEventListener('input', (e) => {
      this.state.iter = parseInt(e.target.value);
      document.getElementById('iterValue').innerText = this.state.iter;
    });
    document.getElementById('iterSlider').addEventListener('change', () => this.requestFrame());

    document.getElementById('resSelect').addEventListener('change', (e) => {
      this.resolutionDivider = parseInt(e.target.value);
      this.requestFrame();
    });

    // Animation Controls
    document.getElementById('btnKeyframe').addEventListener('click', () => this.addKeyframe());
    document.getElementById('btnClearPath').addEventListener('click', () => {
      this.animationPath = [];
      this.renderKeyframeList();
    });
    document.getElementById('btnPlay').addEventListener('click', () => this.playAnimation());
    document.getElementById('btnPlayDefault').addEventListener('click', () => this.playDefaultAnimation());
    document.getElementById('btnExportPath').addEventListener('click', () => this.exportAnimationPath());
    document.getElementById('btnImportPath').addEventListener('click', () => document.getElementById('importFile').click());
    document.getElementById('importFile').addEventListener('change', (e) => this.importAnimationPath(e));
    document.getElementById('btnStop').addEventListener('click', () => this.stopAnimation());

    // Export
    document.getElementById('btnExport').addEventListener('click', () => {
      const link = document.createElement('a');
      link.download = `mandelbrot_${Date.now()}.png`;
      link.href = this.canvas.toDataURL();
      link.click();
    });
  }

  resize() {
    const parent = document.getElementById('viewport');
    this.canvas.width = parent.clientWidth;
    this.canvas.height = parent.clientHeight;
    this.updateAspect();
    this.requestFrame();
  }

  renderLoop() {
    // Simple loop to catch drag updates
    if (this.isDragging && !this.pendingRequest && this.hasMoved) {
      this.requestFrame();
      this.hasMoved = false; // Reset flag until next mousemove
    }
    requestAnimationFrame(() => this.renderLoop());
  }

  updateUI() {
    const now = performance.now();
    const delta = now - this.lastFrameTime;
    this.lastFrameTime = now;

    // If the gap is smaller than 1 second, it's a real frame sequence (dragging/animating).
    // If it's larger, we were just idle, so don't update the display to 0.
    if (delta < 1000) {
      const fps = 1000 / delta;
      document.getElementById('fps').innerText = `FPS: ${Math.round(fps)}`;
    }
    document.getElementById('coords').innerText = `${this.state.x.toFixed(5)} : ${this.state.y.toFixed(5)}`;
    // Calculate abstract zoom level
    const zoom = 3.0 / this.state.w;
    document.getElementById('scale').innerText = `Zoom: ${zoom.toExponential(2)}x`;
  }

  // --- Animation System ---

  addKeyframe() {
    // Deep copy state
    const frame = { ...this.state };
    this.animationPath.push(frame);
    this.renderKeyframeList();

    const btnPlay = document.getElementById('btnPlay');
    if (this.animationPath.length >= 2) btnPlay.disabled = false;
  }

  renderKeyframeList() {
    const list = document.getElementById('keyframeList');
    list.innerHTML = '';
    if (this.animationPath.length === 0) {
      list.innerHTML = '<div class="empty-state">No keyframes</div>';
      document.getElementById('btnPlay').disabled = true;
      return;
    }
    this.animationPath.forEach((kf, idx) => {
      const div = document.createElement('div');
      div.className = 'keyframe-item';
      div.innerText = `Keyframe ${idx + 1} (Iter: ${kf.iter})`;
      list.appendChild(div);
    });
  }

  playAnimation() {
    if (this.animationPath.length < 2) return;
    this.isPlaying = true;
    this.animIndex = 0;
    this.animStartTime = Date.now();
    this.animDuration = 1000; // 2 seconds between keyframes

    // Disable controls and enable stop button
    document.body.style.pointerEvents = 'none';
    document.getElementById('btnStop').disabled = false;

    this.advanceAnimation();
  }

  stopAnimation() {
    if (!this.isPlaying) return;
    this.isPlaying = false;
    document.body.style.pointerEvents = 'auto';
    document.getElementById('btnStop').disabled = true;
  }

  advanceAnimation() {
    if (!this.isPlaying) return;

    const now = Date.now();
    const elapsed = now - this.animStartTime;
    const progress = elapsed / this.animDuration;

    if (progress >= 1.0) {
      // Move to next segment
      this.animIndex++;
      this.animStartTime = Date.now();
      if (this.animIndex >= this.animationPath.length - 1) {
        // End of animation
        this.isPlaying = false;
        document.body.style.pointerEvents = 'auto';
        document.getElementById('btnStop').disabled = true;
        return;
      }
    }

    // Interpolate between animIndex and animIndex + 1
    const start = this.animationPath[this.animIndex];
    const end = this.animationPath[this.animIndex + 1];

    // Linear interpolation (no easing)
    const t = Math.min(progress, 1.0);

    // Linear interpolation for X, Y, Iter
    this.state.x = start.x + (end.x - start.x) * t;
    this.state.y = start.y + (end.y - start.y) * t;
    this.state.iter = Math.floor(start.iter + (end.iter - start.iter) * t);

    // Linear interpolation for dimensions (instead of logarithmic)
    this.state.w = start.w + (end.w - start.w) * t;
    this.state.h = start.h + (end.h - start.h) * t;
    this.updateAspect();

    this.requestFrame();
  }

  // --- Animation Import/Export ---

  exportAnimationPath() {
    if (this.animationPath.length === 0) {
      alert('No animation path to export. Add some keyframes first!');
      return;
    }
    const data = {
      version: '1.0',
      created: new Date().toISOString(),
      keyframes: this.animationPath
    };
    const json = JSON.stringify(data, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.download = `mandelbrot_animation_${Date.now()}.json`;
    link.href = url;
    link.click();
    URL.revokeObjectURL(url);
  }

  importAnimationPath(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const data = JSON.parse(e.target.result);
        if (!data.keyframes || !Array.isArray(data.keyframes)) {
          throw new Error('Invalid animation file format');
        }
        // Validate keyframe structure
        for (const kf of data.keyframes) {
          if (typeof kf.x !== 'number' || typeof kf.y !== 'number' || 
              typeof kf.w !== 'number' || typeof kf.h !== 'number' ||
              typeof kf.iter !== 'number') {
            throw new Error('Invalid keyframe structure');
          }
        }
        this.animationPath = data.keyframes;
        this.renderKeyframeList();
        alert(`Loaded ${data.keyframes.length} keyframes`);
        if (data.keyframes.length >= 2) btnPlay.disabled = false;
      } catch (err) {
        alert('Failed to import animation: ' + err.message);
      }
      // Reset file input
      event.target.value = '';
    };
    reader.readAsText(file);
  }

  playDefaultAnimation() {
    // A nice zoom journey through the Mandelbrot set
    this.animationPath = [
    {
      x: -2.3,
      y: -1.2,
      w: 4,
      h: 2.43142144638404,
      iter: 0
    },
    {
      x: -2.3,
      y: -1.2,
      w: 4,
      h: 2.43142144638404,
      iter: 50
    },
    {
      x: -1.6501847508732521,
      y: -0.2507184953803835,
      w: 0.7520876409271516,
      h: 0.45716050492766336,
      iter: 49
    },
    {
      x: -1.8699529893520397,
      y: -0.15296388875641437,
      w: 0.5135133977756521,
      h: 0.31214187208931415,
      iter: 49
    },
    {
      x: -1.8699529893520397,
      y: -0.15296388875641437,
      w: 0.5135133977756521,
      h: 0.31214187208931415,
      iter: 10
    },
    {
      x: -1.8699529893520397,
      y: -0.15296388875641437,
      w: 0.5135133977756521,
      h: 0.31214187208931415,
      iter: 190
    },
    {
      x: -1.9384171376668917,
      y: -0.09955570932290318,
      w: 0.3301005695268874,
      h: 0.2006534010528145,
      iter: 190
    },
    {
      x: -1.8640482960476545,
      y: -0.0024742214794128505,
      w: 0.007701591264032635,
      h: 0.004681453542663215,
      iter: 190
    },
    {
      x: -1.8621875778760246,
      y: -0.000051509748331205526,
      w: 0.0001264847988182686,
      h: 0.00007688446312207702,
      iter: 190
    },
    {
      x: -1.8621875778760246,
      y: -0.000051509748331205526,
      w: 0.0001264847988182686,
      h: 0.00007688446312207702,
      iter: 110
    },
    {
      x: -1.8621875778760246,
      y: -0.000051509748331205526,
      w: 0.0001264847988182686,
      h: 0.00007688446312207702,
      iter: 950
    },
    {
      x: -1.8621166379815548,
      y: -0.000005436211901879243,
      w: 0.000017861700672238183,
      h: 0.000010857330520843043,
      iter: 0
    },
    {
      x: -1.8621166379815548,
      y: -0.000005436211901879243,
      w: 0.000017861700672238183,
      h: 0.000010857330520843043,
      iter: 80
    },
    {
      x: -1.8621166379815548,
      y: -0.000005436211901879243,
      w: 0.000017861700672238183,
      h: 0.000010857330520843043,
      iter: 180
    },
    {
      x: -1.8621166379815548,
      y: -0.000005436211901879243,
      w: 0.000017861700672238183,
      h: 0.000010857330520843043,
      iter: 760
    }
  ];
    this.renderKeyframeList();
    this.playAnimation();
  }
}

// Start
window.onload = () => {
  new MandelbrotViewer();
};
