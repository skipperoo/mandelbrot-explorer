class MandelbrotViewer {
  constructor() {
    // --- Configuration ---
    this.socketUrl = `ws://${window.location.host}/ws`;
    this.canvas = document.getElementById('mandelbrotCanvas');
    this.ctx = this.canvas.getContext('2d', { alpha: true }); // Optimize for no alpha

    // --- State ---
    // Initial view: Standard Mandelbrot center
    this.state = {
      x: -2.3, // x_min
      y: -1.2, // y_min
      w: 4.0,  // Viewport width in math coords (controls x_scale)
      h: 4.0,  // Viewport height in math coords (calculated from aspect)
      iter: 1000
    };

    this.isDragging = false;
    this.lastMouse = { x: 0, y: 0 };
    this.resolutionDivider = 1; // 1 = full res, 2 = half res (for speed)
    this.animationPath = [];
    this.isPlaying = false;
    this.socket = null;
    this.pendingRequest = false;
    this.maxIterations = 10000;
    this.hasMoved = false;
    this.lastFrameTime = performance.now();

    // Generate the Blue Ocean Palette
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
    this.renderLoop(); // Start the render trigger loop
  }
  generatePalette(themeName) {
    this.currentTheme = themeName || 'ocean'; // Default to ocean

    for (let i = 0; i <= this.maxIterations; i++) {
      // Base case: Inside the set is always Black
      if (i === this.maxIterations) {
        this.palette[i] = 0xFF000000;
        continue;
      }

      let r, g, b;
      // Normalize iteration count. 
      // Multiplier '0.1' determines how fast the colors cycle (lower = wider bands)
      const t = i * 0.1;

      switch (this.currentTheme) {
        case 'fire':
          // Red and Gold
          r = Math.floor((0.6 + 0.4 * Math.cos(t)) * 255);
          g = Math.floor((0.4 + 0.3 * Math.cos(t + 1.0)) * 255);
          b = Math.floor((0.2 + 0.2 * Math.cos(t + 2.0)) * 255);
          break;

        case 'matrix':
          // Neon Green & Black
          r = 0;
          g = Math.floor((0.6 + 0.4 * Math.sin(t * 0.5)) * 255);
          b = 0;
          if (g < 60) g = 0; // Create stark black bands
          break;

        case 'rainbow':
          // Full spectrum
          r = Math.floor((0.5 + 0.5 * Math.sin(t)) * 255);
          g = Math.floor((0.5 + 0.5 * Math.sin(t + 2.09)) * 255);
          b = Math.floor((0.5 + 0.5 * Math.sin(t + 4.18)) * 255);
          break;

        case 'zebra':
          // Black and White Stripes for debugging
          const v = (i % 20) < 10 ? 255 : 0;
          r = g = b = v;
          break;

        case 'ocean':
        default:
          // FIXED BLUE OCEAN MATH
          // Red: kept very low (0-50 range) to prevent yellowing
          r = Math.floor(25 + 25 * Math.cos(t));

          // Green: varies mid-range to create Teal/Cyan
          g = Math.floor(100 + 80 * Math.sin(t * 0.5));

          // Blue: kept high (150-255)
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
      // Auto reconnect after 3s
      setTimeout(() => this.connect(), 3000);
    };

    this.socket.onmessage = (event) => {
      this.handleResponse(event.data);
    };
  }

  // --- Core Logic: Coordinate Systems ---

  // Updates internal state based on pixel resolution
  updateAspect() {
    const aspect = this.canvas.width / this.canvas.height;
    // Keep x_scale fixed, adjust y_scale (height) to match aspect
    // This prevents stretching when window resizes
    this.state.h = this.state.w / aspect;
  }

  getParams() {
    // Current actual resolution requested
    const width = Math.floor(this.canvas.width / this.resolutionDivider);
    const height = Math.floor(this.canvas.height / this.resolutionDivider);

    // Math Scale per pixel
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

  // --- Networking ---

  requestFrame() {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) return;
    if (this.pendingRequest) return; // Flow control: don't flood server

    const p = this.getParams();
    // Protocol: "x_min,y_min,x_scale,y_scale,width,height,iterations"
    const msg = `${p.x_min.toFixed(15)},${p.y_min.toFixed(15)},${p.x_scale.toFixed(15)},${p.y_scale.toFixed(15)},${p.width},${p.height},${p.iterations}`;

    this.socket.send(msg);
    this.pendingRequest = true;
    document.getElementById('loading').classList.remove('hidden');
  }

  handleResponse(arrayBuffer) {
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

    // 1. SAVE DATA: Clone the buffer so we can re-use it for theme changes
    // We use Uint16Array to interpret the raw bytes
    this.lastIterations = new Uint16Array(arrayBuffer);

    // 2. Trigger Draw
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
    for (let k = 0; k < len; k++) {
      pixelView[k] = this.palette[this.lastIterations[k]];
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
      // 1. Update Palette Table
      this.generatePalette(e.target.value);
      this.draw();
    });
    this.canvas.addEventListener('dblclick', e => {
      e.preventDefault();

      // 1. Reset to initial values (from constructor)
      this.state.x = -2.0;
      this.state.y = -1.5;
      this.state.w = 3.0;

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
    this.animDuration = 2000; // 2 seconds between keyframes

    // Disable controls
    document.body.style.pointerEvents = 'none';

    this.advanceAnimation();
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
        return;
      }
    }

    // Interpolate between animIndex and animIndex + 1
    const start = this.animationPath[this.animIndex];
    const end = this.animationPath[this.animIndex + 1];

    // Ease function (Smoothstep)
    const t = Math.min(progress, 1.0);
    const ease = t * t * (3 - 2 * t);

    // Linear interpolation for X, Y, Iter
    this.state.x = start.x + (end.x - start.x) * ease;
    this.state.y = start.y + (end.y - start.y) * ease;
    this.state.iter = Math.floor(start.iter + (end.iter - start.iter) * ease);

    // Logarithmic interpolation for Zoom (Scale)
    // scale = start * (end/start)^t
    // Width 'w' acts as the inverse of scale.
    // w(t) = w0 * (w1/w0)^t
    const wRatio = end.w / start.w;
    this.state.w = start.w * Math.pow(wRatio, ease);
    this.updateAspect();

    this.requestFrame();
  }
}

// Start
window.onload = () => {
  new MandelbrotViewer();
};
