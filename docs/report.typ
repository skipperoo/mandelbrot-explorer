#import "utils.typ": *
#import "data.typ": *

// ######################## Document Setup ########################

#set page("a4")
#set par(justify: true)
#set heading(numbering: (..nums) => {
  if nums.pos().len() <= 3 {
    numbering("1.1", ..nums)
  }
})




#show heading.where(level: 1): it => {
  if counter(heading).get().first() > 1 {
    pagebreak()
  }
  it
}

#show heading.where(level: 4): it => pad(left: -0.3em, it)
#align(center, text(17pt)[
  *Advanced Programming*
])
#align(center)[
  Leonardo Scoppitto\
  February 2025
]


#pagebreak()
#outline(depth: 3)
#set page(numbering: none)
#pagebreak()
#counter(page).update(1)
#set page(numbering: "1", number-align: center)




= Design phase

The project I chose is _B09 - AI-Assisted Multithreaded Mandelbrot Viewer with Zoom, Export, and Path Animation Goal_.

#rounded-box[
```markdown
# B09 - AI-Assisted Multithreaded Mandelbrot Viewer with Zoom, Export, and Path Animation Goal

Use AI to help design and implement a multithreaded Mandelbrot set viewer that supports interactive zooming, image export, and animation along a user-defined path into the fractal (e.g. zoom flight), leveraging concurrency to render efficiently.

## Tasks
1. Design & Implement: Use AI to assist in implementing the Mandelbrot renderer (coloring, iteration control) with multithreaded rendering, an interactive UI for panning/zooming, image export (e.g. PNG), and an animation feature that interpolates a path of viewpoints and generates frames or a video-like sequence.

2. Test & Demonstrate: Demonstrate interactive usage (smooth zooming and navigation), validate correctness of the fractal rendering, measure/observe the impact of multithreading, and showcase at least one non-trivial animation path into the fractal.

3. AI Usage & Verification Report: Collect and submit the prompts you used with AI and write a short report explaining how you evaluated and corrected the AI-generated code (thread-safety, performance, correctness of the math, and quality of exported images/animations).
```
]

I already had an idea on how I wanted to implement this project, here's a summary of the technical requirements that I imposed to the LLMs:


*Requirement \#1*
#pad(left: 1em)[
  The first requirement is *not to use third party libraries*. One of the benefits of using AI to speedup development is that, for simple and non critical use casesit is almost free to reinvent the wheel#footnote[Of course core and critical libraries such as OpenSSL, complex protocol libraries or DB connectors are excluded from this argument.] and avoid the dependency hell that plagues modern software development#footnote[https://en.wikipedia.org/wiki/Dependency_hell.].
]

*Requirement \#2*
#pad(left: 1em)[
  Implement the project using a client-server architecture to decouple the frontend and the backend.
]

*Requirement \#3*
#pad(left: 1em)[
  For the frontend I decided to implement a minimal web app so that with one codebase I could target any device with a modern browser. To limit the boilerplate at the minimum I opted for plain HTML + JS + CSS instead of framework like React, which is suitable for more complex application with a complex state management.
]

#pagebreak()

*Requirement \#4*
#pad(left: 1em)[
  For the backend I wanted to be able to easily use SIMD instructions to speedup the rendering and to implement GPU rendering, so the candidates were C, C++, Rust and Zig. In Table 1 I reported what I have considered when choosing the language. Ultimately, I opted for C because I worked with it professionally and because I prefer it for simple projects that require high performance and low level programming.
]

#figure(
  table(
    columns: (auto,) * 3,
    align: left+horizon,
    [*Language*], [*Pros*], [*Cons*],
    [C], [
      - Simple
      - Well-known by LLMs
      - Stable SIMD support via `immintrin.h`
      - Stable OpenGL library
    ], [
      - Manual memory management
      - Rudimental thread support
    ],

    [C++], [
      - Well-known by LLMs
      - Support for promises and futures
      - Stable SIMD support via `immintrin.h`
      - Stable OpenGL library
    ], [
      - Good for big enterprise projects, but unnecessary complex for smaller projects
    ],

    [Rust], [
      - Memory safe
      - Safe concurrency support via `Arc` wrappers and async multithreaded `tokio` runtime
      - Compiler hints help the LLMs (and agents) to iterate and fix the code faster
    ], [
      - SIMD support is still experimental
      - Harder and more complex to write and to review
      - OpenGL wrappers need `unsafe` blocks
      - I have limited experience with it
  ],

    [Zig], [
      - Built-in advanced memory management via allocators
      - Better memory safety via `Optional` type and `defer/errdefer` mechanism
      - SIMD support out-of-the-box via `@Vector`
      - OpenGL support via `C` interop functionality
    ], [
      - It is still a relatively new language, so I do not expect LLMs to be as good as they could be with a more mainstream language
      - I have zero experience with this language
  ],
  ),
  caption: [Comparison between languages]
)



= Implementation

== Chat based 

To structure the project and set the foundation of the renderer and the visualizer, I used *Gemini 3 Pro* since it is free to use with the `@unipi` account.

#rounded-box[
#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    #emph[
      Hello I have a implement this college project project:

      Multithreaded Mandelbrot Viewer with Zoom, Export, and Path Animation along a user-defined path into the fractal (e.g. zoom flight), leveraging concurrency to render efficiently.

      I have a basic idea on how to architecture this:
      - Frontend: Basic `HTML + JS + CSS` to be able to view the mandelbrot. In a first iteration hw acceleration is not mandatory. We will discuss this later if there is time.
      - Backend: I want to leverage SIMD (Intel AVX/AVX2) instructions to speedup the computation, other than using multithreading, so I was thinking about a C server that does the heavy-lifting of the computation.

      Now, the two components have to talk each other so I was thinking about websockets or better webRTC, but I can evaluate other options if you think are worth it.

      An important rule: this has to be a ZERO dependency project, everything must be implemented from scratch, even the websocket/webrtc communication between the backend and the frontend

      Please evaluate this proposal, highlight the strong and weak points and help me write down the implementation roadmap.
    ]
  ]
)
]



#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    Gemini basically approved the tech stack and the constraints, suggesting to use websockets as it's easier to implement than WebRTC.

    To visualize the madelbrot it suggested the canvas API (`<canvas>` tag) and it also designed the basic communication protocol:

    - The client sends [`x_min`,`y_min`,`x_scale`,`y_scale`,`width`,`height`,`iterations`] as a CSV string (I added the iteration count later as it was fixed).
    - The server responds with an `ArrayBuffer` containing the bitmap to be painted on the `<canvas>`.
  ]
)
]
]


#rounded-box[
#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    #emph[
      Let's start with the math part. Please create a `render_simd` function to leverage `AVX` instruction, conditionally for the correct target architecture (I have to test it on my laptop with `AVX2` and on my server with `AVX`) and create a main function to benchmark the speedup when using single core, multicore and multicore + SIMD
    ]
  ]
)
]


#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    Created successfully a benchmark program to test the every combination (No SIMD single/multithreaded, SIMD single/multithreaded).
  ]
)
]

#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    #emph[
      Go on implementing the WebSocket accept function, implementing the base64 encode and SHA1 checksum from scratch.
    ]
  ]
)
]


#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    Created the two utility functions correctly and the library to upgrade an HTTP connection to a websocket, implementing also the `send` and `receive` functions.
  ]
)
]


#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    #emph[
      Please finish the implementation creating the main server loop to accept requests from multiple clients and add a threadpool to render the mandelbrot.
    ]
  ]
)
]

#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    It completed the backend application with the main loop so that I could start implementing the frontend.
  ]
)
]

]

#pagebreak()

At this point I started a new chat to prevent the LLM from hallucinating#footnote[It started deviating from the previous context using functions that didn't exist.] as it can happen with longer chats.

#rounded-box[
#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    `A bit of context about the project`

    #emph[
      Let's start the frontend implementation. These are the rules:
      - Zero dependency: the frontend shouldn't require any external library
      - Use pure `HTML5 + JS + CSS`, no frameworks.
      - The style should be minimal and modern (dark theme Shadcn like)
      - Remember the specification: Mandelbrot renderer with coloring, iteration control, an interactive UI for panning/zooming, image export (e.g. PNG), and an animation feature that interpolates a path of viewpoints and generates frames or a video-like sequence
      - The protocol to be used over the websocket is:
    ]
    ```c
    // Expected format: "x_min,y_min,x_scale,y_scale,width,height,iterations"
    sscanf(payload, "%lf,%lf,%lf,%lf,%d,%d,%d", &x_min, &y_min, &x_scale,
           &y_scale, &width, &height, &iterations)  
    ```
    #emph[
      - The data is returned as an `ArrayBuffer` so the receive part should look something like this:
    ]
    ```js
    const socket = new WebSocket(`ws://${baseUrl}:8080`);
    socket.binaryType = 'arraybuffer';

    socket.onopen = () => {
        // Send initial coords
        socket.send("-2.0,-1.5,0.005,0.005,800,600");
    };

    socket.onmessage = (event) => {
        const arrayBuffer = event.data;
        const uint8Array = new Uint16Array(arrayBuffer);
        // Put this data into an HTML5 Canvas ImageData
    };
    ```
  ]
)
]


#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    It created the `index.html`, the `style.css` and the main application `script.js`. The application opened in a browser seems to be fine, apart from minor UI inconsistencies that I manually fixed very quickly.
  ]
)
]
]

#rounded-box[
#pad(left: 5em)[
#box(
  fill: gray.lighten(70%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    #emph[
      Can you create a makefile based on this structure to compile the project?
      ```
      .
      ├── Makefile
      └── src
         ├── include
         │  ├── mandelbrot.h
         │  ├── tpool.h
         │  ├── websocket.h
         │  └── websocket_utils.h
         ├── mandel_bench
         ├── mandelbrot.c
         ├── server.c
         ├── tpool.c
         ├── websocket.c
         └── websocket_utils.c
      ```
    ]
  ]
)
]


#pad(right: 5em)[
#box(
  fill: gray.lighten(80%),
  //stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  [
    The Makefile was almost ok, except for a couple of compilation flags such as `-mfma` and `-mavx2`, which prevented the code from compiling on the older architecture of my server, so I changed them to `-march=native`, enabling all the optimization available on the machine that compiles the code, since the `render_simd` function automatically compiles to use the best available SIMD instruction set.
  ]

)
]
]

#pagebreak()
== First troubleshooting

The code, *unexpectedly* compiled right away and the frontend tried to connect to the backend, but the backend crashed with a `segfault` a moment after the connection.

Inspecting the code manually revealed two problem:

1. The `buffer` is declared as an array of `4096` elements and set to `0`, but on line `10`, the read function is allowed to overwrite the null terminator in the last position of the array. Since the buffer is later manipulated by `string.h` functions that expect the input string to be null terminated, I changed the buffer declaration to `char *buffer[4097]`. However, this was not the bug that made the backend crash.
#pretty_code[
  ```c
    char *buffer[4096] = {0};
    // ...
    client_fd =
        accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
    if (client_fd < 0)
      continue;

    printf("Client connected\n");
    memset(buffer, 0, 4096);
    read(client_fd, buffer, 4096);
    printf("Received: %s\n", buffer);
    if (perform_handshake(client_fd, buffer) != 0) {
      close(client_fd);
      continue;
    }
    printf("Handshake successful\n");
```
]
2. I  manually isolated the function that caused the crash and it was the `SHA1` implementation, where an overflow when computing `part_len` caused the `memcpy` try to copy terabytes of data. The LLM provided an update version that resolved the issue.

#pretty_code[
  ```c
void sha1(const unsigned char *data, size_t len, unsigned char *hash) {
  size_t i, j;
  for (i = 0; i < len + 9; i += 64) {
    unsigned char block[64] = {0};
    size_t part_len = (i + 64 > len) ? len - i : 64;
    memcpy(block, data + i, part_len);
    // ...
  }
  // ...
}
```
]

At this point, the core of the project is working as expected and I can focus on details and new features.

#pagebreak()

== AI agents, Opencode and Gemini CLI

Once the project worked as it should, I took some time to review and audit the code:

- Check for redundancies and unused code
- Reduced the verbosity of some functions

Then, I containerized the application with docker to easily build and test it with few commands and have a stable reproducible test environment. So, I deployed:

- A container running the backend
- A container running Nginx to serve the frontend and proxy the requests to the backend

#pretty_code(label: "First docker-compose.yml")[
  ```yml
services:
  backend:
    build: ./backend
    restart: unless-stopped
    environment:
      - MANDELBROT_THREADS=12 
      - MANDELBROT_SIMD=1 # Use AVX instructions set
    expose:
      - 8080

  frontend:
    image: nginx:latest
    restart: unless-stopped
    ports:
      - 8080:80
    volumes:
      - ./nginx:/etc/nginx
  ```
]

At this point, I had a reproducible environment to manually test the application and the instructions to let the agent build and run the application, so the next step was to initialize a git repository to track the changes made by the agent and create a new branch, which I named `feature/opengl-support` as I wanted to implement the support for rendering the mandelbrot using the GPU. I opted for OpenGL because is well supported by the C language via CHECK LIB NAME and it can target both the Intel and NVIDIA GPUs with just one implementation.

First, I attempted using #link("https://opencode.ai/")[Opencode] wired to a local instance of `qwen3:8b` running on Ollama on one of my servers, but the GTX1070 with only 8Gb of VRAM was not enough to get a good development experience, resulting in an high waiting time before the first token and an output that was not on par with much larger models. Then I tried to connect my Gemini account to both Opencode and #link("https://geminicli.com/")[Gemini CLI], which worked flawlessly.

Before issuing any task to the agent, I took some time review the content of the two main Gemini chats I used to create the foundations of the project which has been summarized into `docs.md`.

I created a new specification file that outlined the new feature to be developed, specifying the function signature, the new environment variable to be used to enable GPU rendering and the regression and acceptance tests#footnote[Basically running the backend benchmark and then start the server and send some rendering requests using a python script.] that the agent should perform in order to decide whether the implementation was acceptable or not. For the first iteration I tried `Big Pickle`, a free model available in Opencode:

#box(
  stroke: 1pt + black,
  inset: 10pt,
  radius: 5pt,
  [
  `> Please read` #text(blue.lighten(20%))[`@docs.md`] `to get a context of what we're doing and then read` #text(blue.lighten(20%))[`@task.md`] `to implement the OpenGL support`
]
)

```
# Todos
[✓] Add OpenGL headers to mandelbrot.h
[✓] Implement render_opengl function in mandelbrot.c
[✓] Add GPU environment variable support
[✓] Update Makefile to link OpenGL libraries
[✓] Configure docker-compose.yml for GPU passthrough
```
It successfully created the harness to make the function work, but the actual implementation used the scalar rendering even if it reported the successful implementation of GPU support:

#pretty_code()[
  ```c
  void render_opengl(uint16_t *buffer, int width, int start_row, int end_row,
                    int max_iterations, double x_min, double y_min,
                    double x_scale, double y_scale, RenderMode mode) {

    render_scalar(buffer, width, start_row, end_row,
                  max_iterations, x_min, y_min, x_scale,
                  y_scale);

  }
  ```
]

Clearly, a free model could not compete with a pro model like Gemini, so I decided to hand the task to Gemini CLI, using the Gemini 3 Pro (high reasoning) to plan the steps needed to fix the code and make it support OpenGL and then I used Gemini 3 Flash (still high reasoning) to write the actual implementation, as it is faster and uses less tokens#footnote[The Unipi plan offers about 2M free tokens every 24 hours.].
With a bit of back and forth, I managed to direct Gemini towards the solution analyzing the logs together.

Now I wanted to get rid of Nginx, as the project just needs a very basic webserver that serves the HTML file (i.e. responds to basic GET requests). I created another `task.md` file (see appendix N). Gemini managed to build it in one shot as it is a fairly easy task, also handling corner cases and preventing traversal attempts as I instructed it to do in the specification.

= Multithreaded performance evaluation
The performance evaluation reveals distinct trends across CPU and GPU configurations. For CPU rendering, performance scales inversely with workload intensity (represented by zoom levels 0.5, 1, and 5), with SIMD (AVX) implementations consistently outperforming Scalar variants by approximately 3-4x across all resolutions. The impact of thread pinning is workload-dependent: for the lighter "05" zoom level, keeping thread pinning OFF yields significantly better Scalar results (reaching ~24.36 FPS vs ~17.63 FPS with pinning ON). However, for heavier workloads like zoom level "5", enabling thread pinning provides a slight advantage in SIMD performance (rising to 3.69 FPS from 3.39 FPS). On the GPU side, the discrete Nvidia GPU demonstrates a massive advantage over the integrated Intel GPU, delivering roughly 3x higher frame rates at 1080p resolution across all tested scenarios (e.g., ~61 FPS vs ~17 FPS in benchmark 1).

SIMD Processing Efficiency The significant performance jump seen in the CPU results is due to the backend's use of AVX/AVX-512 instruction sets. Specifically, the AVX2 implementation utilizes YMM registers to process 4 double-precision pixels per instruction, while AVX-512 uses ZMM registers to handle 8 pixels simultaneously. This "vertical" SIMD processing allows the CPU to calculate the escape time for multiple neighboring pixels in a single cycle. This architectural choice explains why the SIMD results are consistently several times faster than the Scalar implementation across all benchmarks.

Thread Pinning and Workload Intensity Thread pinning impacts performance differently depending on the workload duration. In the lighter "05" benchmark, disabling pinning yields better results (e.g., ~24.36 FPS vs ~17.63 FPS at 540p). This suggests that for fast-rendering frames, the overhead of strictly managing thread affinity outweighs the benefits, and the OS scheduler handles the rapid tasks more efficiently. However, in the heavier "5" benchmark, pinning provides a tangible advantage (e.g., ~3.69 FPS vs ~3.39 FPS at 540p SIMD). Here, the threads run for longer durations, making the improved cache locality and reduced context switching provided by pinning more valuable than the scheduling flexibility.

GPU Performance on Heavy Workloads The Intel GPU shows an interesting trend where it performs better on the heavier "Benchmark 5" (~19.68 FPS at 1080p) compared to the medium "Benchmark 1" (~16.98 FPS at 1080p). This behavior is likely linked to the system's use of GL_MAP_PERSISTENT_BIT to map GPU memory directly to the CPU's address space. In heavier workloads, the GPU spends more time calculating per pixel relative to the time spent on memory synchronization and driver overhead. This higher ratio of "compute" to "overhead" allows the GPU to remain saturated more effectively. Additionally, the rendering engine automatically optimizes precision, switching between float and double based on zoom levels to maximize throughput, which helps maintain performance even as the workload complexity increases.
Although the benchmark data generally shows linear or sub-linear scaling, instances of superlinear speedup (where performance increases by a factor greater than the increase in resources, such as the ~50% jump in performance when moving from 6 to 8 threads in the pinned benchmark ) can be attributed to Cache Aggregation and Load Balancing efficiency.

    Cache Aggregation: In the architecture, each CPU Worker thread calculates a portion of the image and writes to a "Shared Buffer". With a single thread, the working set might exceed the local L1/L2 cache size, causing frequent, slow main memory access. When utilizing multiple threads, the total available high-speed cache memory effectively multiplies (e.g., 8 cores provide 8x the L2 cache). This allows the specific image slices being processed by the "Thread Pool" to fit entirely within the CPU caches, drastically reducing memory latency and enabling the SIMD units to run at full capacity without waiting for data.

    Workload Distribution: The rendering engine includes an optimization that "checks for periodicity... to exit early", making the computational cost per pixel highly variable (pixels in the black cardioid are much faster than those at the fractal edge). With more threads available in the pool, the "heavy" slices are less likely to become a bottleneck that stalls the entire frame. The statistical probability of distributing these expensive chunks efficiently improves with the number of workers, potentially yielding performance gains that exceed simple core scaling.

Mention that for SIMD we process 4 pixels at a time so the ideal speedup should be 4 times ideal speedup. That said, there are operations that are not SIMD accelerated (buffer copy comparison etc) so it does not scale super linearly.

COMPARE EVERYTHING WITH THE SAME THREAD ARCHITECTURE AND THEN DO THE COMPARISON AND SHOW THAT THE IDEAL SCALEUP OF 4xSCALAR IS ALMOST THERE

#let show_cpu_benchmark(no_pinning_data, pinning_data, caption, normalized: false) = {
  pagebreak()
  let counter = context { counter(figure.where(kind: "benchmark")).get().at(0)+1 }
  align(center)[
    #text(size: 16pt)[
      Benchmark #counter: #caption
    ]
  ]
  figure(
    grid(
      columns: 1,
      row-gutter: 1em,
      grid(
        columns: (auto,)*2,
        column-gutter: 3em,
      if normalized {plot_cpu_performance_normalized(no_pinning_data)} else {plot_cpu_performance(no_pinning_data)},
      if normalized {plot_cpu_performance_normalized(pinning_data)} else {plot_cpu_performance(pinning_data)},
      ),
      if normalized {table_cpu_performance_normalized(no_pinning_data)} else {table_cpu_performance(no_pinning_data)},
      if normalized {table_cpu_performance_normalized(pinning_data, tp: true)} else {table_cpu_performance(pinning_data, tp: true)},
    ),
    supplement: [Benchmark],
    kind: "benchmark"
  )
}


#let show_gpu_benchmark(data, caption) = {
  let counter = context { counter(figure.where(kind: "benchmark")).get().at(0)+1 }
  align(center)[
    #text(size: 16pt)[
      Benchmark #counter: #caption
    ]
  ]
  figure(
    plot_grouped_gpu_performance(data, labels: ("ZOOM=0.5","ZOOM=1", "ZOOM=5"))
  )
}
#show_cpu_benchmark(benchmark_05, benchmark_05_TP, [Run with `ZOOM=0.5`])
#show_cpu_benchmark(benchmark_1, benchmark_1_TP, [Run with `ZOOM=1`])
#show_cpu_benchmark(benchmark_5, benchmark_5_TP, [Run with `ZOOM=5`])

#show_cpu_benchmark(benchmark_05, benchmark_05_TP, [Run with `ZOOM=0.5`. SIMD speedup uses the single thread scalar execution as the baseline], normalized: true)
#pagebreak()
#show_gpu_benchmark((benchmark_05,benchmark_1, benchmark_5), [GPU])




= Conclusion

Before starting the project, I already knew a bit of all the technologies that I ended up using, but surely I wouldn't have been able to implement all these features in such a small amount of time.
AI clearly helped me to deep dive into each aspect of the project where I wasn't so knowledgable#footnote[See Appendix 2 for a quick summary of what I learned implementing this project.] and also speeded up the implementation part, even when I could have been perfectly capable of writing the same code.

Regarding the development experience, a distinction between chatbots and agents has to be made:
- Going back and forth between the chat and the editor is useful in the researching phase as it is a more natural interface to interact with an LLM to brainstorm ideas, but it is not so good when you have to start implementing things seriously.
- Using a coding agent, #underline[given that you provide extended and well written requirements], is extremely powerful, as it can accomplish much more that a chatbot. Moreover, if it can use well structured test and have access to verbose compilation logs, it can work for longer trying to fix bugs and implement features one after the other without human interaction.

That said, while AI is undeniably one of the most powerful productivity tools when it comes to coding, a grain of salt is needed when using it:
- Both chatbots and agents *need* human supervision, diligent code hygiene practices and a good versioning system, as subtle bugs can slip into the code at any time and things can break unexpectedly after the wrong prompt. I think its still crucial that each line produced must be audited and reviewed, even if solid tests are in place, making necessary to have constant review sessions, otherwise the amount of produced code can quickly become unmanageable. Some says that every line of code is both a cost and a liability, so we can say that AI creates liabilities at the speed of light.
- Even if the developer is still the owner of the blueprint and the requirements, navigating the codebase becomes harder than before, as the implementation constantly evolves at high speed, which combined with what said above, poses two scenarios:
  - The speed introduced by AI is bottlenecked by the review speed of humans
  - The code is not fully audited and humans lose track of the codebase
  Given the increasing amount of bugs I'm experiencing using digital devices, sadly, I think the second scenario is the one's winning.
- In my opinion, while it democratizes knowledge and gives access to an infinite amount of resources, it makes the learning experience optional and less effective, as one could be tempted to get answers fast and go directly to the solutions without reasoning about things.

To conclude, I think AI is a revolutionary technology, but, as of now, I think it is not ready to be widely adopted in production environments, except as an enhanced Google Search, especially for junior figures. On the other hand, I think that it shouldn't be used by younger students if not supervised or in a controlled manner, to avoid degrading the learning opportunities.

= Appendix 1: Code snippets and spec files

== Webserver Task

#pretty_code[
  ```markdown
## 2. Next features

As of now the project is composed by a frontend and a backend. The frontend is served by an nignx instance and consists in 3 static files:
- index.html
- script.js
- style.css

Now we want to:
- [] implement a webserver in our backend.
- [] make the backend serve both the mandelbrot generation and the webpages, being aware to implement the basic security features such as avoid use after free, no buffer overflows and so on.
- [] the amount of requests is small so the webserver should run in its own thread with its own request queue.
- [] The page should be served from `/var/www` and the requests should not escape from that path.

A note on the backend project structure:

.
├── benchmark.log <- ignore it
├── Dockerfile <- dockerfile without nvidia framework
├── Dockerfile.nvidia <- dockerfile with nvidia framework
├── frontend <- frontend directory
│  ├── index.html
│  ├── script.js
│  └── style.css
├── Makefile <- makefile for compilation
├── plot_benchmark.py <- ignore it
├── src <- C backend source
│  ├── include <- headers
│  ├── mandelbrot.c <- mandelbrot rendering
│  ├── server.c <- entrypoint and server implementation
│  ├── tpool.c <- mandelbrot threadpool implementation (for the webserver use another thread)
│  ├── webserver.c <- Implement webserver HERE
│  ├── websocket.c <- websocket handshake and send/recieve
│  └── websocket_utils.c <- websocket  utils functions
└── test_one_frame.py
```

]

= Appendix 2: Lesson learned and concepts

== Websockets from scratch

WebSockets provide a full-duplex communication channel over a single TCP connection. The protocol begins with an HTTP handshake:
1. The client sends an upgrade request containing a `Sec-WebSocket-Key`. 
2. The server takes this key, appends a specific GUID string, computes the SHA-1 hash, and returns the base64-encoded result in the `Sec-WebSocket-Accept` header.
3. Once established, data is exchanged in frames. Each frame contains control bits (fin, opcode), a payload length (which can be extended), a masking key (for client-to-server messages), and the payload itself.

In this project, the implementation is handled in `src/websocket.c` and does not rely on external libraries:
- *Handshake:* The `perform_handshake` function searches for the `Sec-WebSocket-Key` header, concatenates it with the magic GUID `258EAFA5-E914-47DA-95CA-C5AB0DC85B11`, computes the SHA-1 hash, and Base64 encodes the result to complete the upgrade.
- *Frame Parsing:* `receive_frame` interprets the first two bytes to determine the opcode (checking for connection close `0x8`) and payload length. It handles variable-length payloads (7-bit, 16-bit extended, or 64-bit extended) and unmasks the client data using the 4-byte masking key (`buffer[i] ^ mask_key[i % 4]`).
- *Binary Responses:* The `send_binary_frame` function constructs valid WebSocket frames with the `FIN` bit set and opcode `0x2` (Binary), ensuring large image buffers are transmitted correctly by setting the appropriate extended payload length bytes in big-endian order.

== Webserver from scratch

Since I've never implemented a Webserver from scratch in C (only in Java), I decided to implement it since the features required for the project to work were minimal. This makes the project self contained and easier to deploy both as a system service or as a Docker container.

The `src/webserver.c` implementation features:
- *Request Parsing:* A dedicated worker thread (`webserver_worker`) parses the raw HTTP request string using `sscanf` to extract the method and path.
- *Security:* The `serve_file` function enforces a root directory jail by checking for ".." substring presence, returning `403 Forbidden` if a traversal attempt is detected.
- *Concurrency:* A producer-consumer queue (`webserver_enqueue`) allows the main thread to offload file serving tasks to the webserver thread, preventing static file I/O from blocking the Mandelbrot rendering loop.

== The Mandelbrot Set: Definition and Algorithms

The Mandelbrot set is defined as the set of complex numbers $c$ for which the sequence $z_(n+) = z_n^2 + c$ (starting with $z_0 = 0$) does not diverge to infinity. Mathematically, if $|z_n| > 2$, the sequence is guaranteed to diverge. The "escape time" algorithm iterates this formula up to a maximum limit; the number of iterations reached determines the pixel's color.

The algorithm is implemented in `src/mandelbrot.c`:
- *Scalar Loop:* The `render_scalar` function implements the classic escape time algorithm. It uses the optimization $x^2 + y^2 < 4.0$ to avoid expensive square root calculations.
- *Algebraic Simplification:* The update rule is expanded to $x_("new") = x^2 - y^2 + x_0$ and $y_("new") = 2 x y + y_0$. By maintaining $x^2$ and $y^2$ as separate variables in the loop, the implementation minimizes the number of multiplication operations per iteration.

=== Threadpool Architecture
To utilize multi-core processors efficiently without the overhead of constantly creating and destroying threads, a threadpool was implemented.
I decided not to use something like OpenMP APIs because, I wanted more flexibility than using OMP directives.

Implemented in `src/tpool.c`:
- *Synchronization:* The `tpool_t` structure uses a `pthread_mutex_t` to protect the task queue and a `pthread_cond_t` to signal sleeping worker threads when new work is available.
- *Task Queue:* A circular buffer holds function pointers (`thread_task_t`). When the main loop needs to render an image, it decomposes the frame into horizontal strips and pushes these tasks to the pool via `tpool_add_work`.
- *Worker Loop:* Threads run an infinite loop (`thread_worker`), blocking on the condition variable until a task is available, executing it, and then returning to sleep, ensuring low latency and high CPU utilization.

=== SIMD and Parallel Computation

To speed up the mandelbrot computation I decided to employ the techniques learned attending the _Distributed systems: paradigms and models_ course.
Single Instruction, Multiple Data (SIMD) allows a processor to perform the same operation on multiple data points simultaneously on the same ALU. The project leverages AVX2 (Advanced Vector Extensions) to process four double-precision floating-point numbers at once. To maximize compatibility, the instruction sets supported are `AVX`, `AVX2` and `AVX-512`, but unfortunately I do not have the hardware to test the latter instruction set, so I only audited the code manually.

The `render_simd` function in `src/mandelbrot.c` holds the SIMD implementation of the mandelbrot rendering:
- *Vectorized Math:* It uses intrinsic functions like `_mm256_mul_pd` and `_mm256_add_pd` to perform the complex number arithmetic on four pixels simultaneously (`__m256d`).
- *Conditional Masking:* Since different pixels diverge at different speeds, standard `if/break` logic cannot be used. Instead, `_mm256_cmp_pd` creates a comparison mask. The iteration count is updated using `_mm256_sub_epi64` only for those elements in the vector that satisfy the condition ($|z|^2 \le 4$).
- *Packing:* The final 64-bit integer counters are packed down to 16-bit integers using permutation (`_mm256_permutevar8x32_epi32`) and packing (`_mm_packus_epi32`) instructions for efficient memory storage.

=== OpenGL and GPGPU Abstraction

Finally, I wanted to dive in GPU programming as I never tried to run any algorithm on a graphic accelerator. To implement this part I instructed the agent to use OpenGL since I knew it supports multiple GPUs vendors, meaning that with one algorithm I could render the mandelbrot both on NVIDIA and Intel GPUs (the one I have in my laptop). 
OpenGL is a cross-language, cross-platform API for rendering vector graphics. While primarily designed for visual output, it can be used for General-Purpose Computing on Graphics Processing Units (GPGPU).

The system abstracts the GPU hardware in `src/mandelbrot.c`:
- *Compute Shaders:* A GLSL compute shader (`#version 430 core`) handles the heavy lifting. It runs one thread per pixel (`gl_GlobalInvocationID`) and implements the Mandelbrot logic, including a geometric optimization that skips calculations for points inside the main cardioid.
- *Persistent Mapping:* To minimize driver overhead, the project uses `glMapBufferRange` with the `GL_MAP_PERSISTENT_BIT`. This maps GPU memory directly into the CPU's address space (`gl_mapped_buffer`), allowing the application to read back render results without explicit buffer copy commands.
- *Vendor Agnostic:* The `init_opengl_for_vendor` function dynamically scans available EGL devices to find and initialize the correct GPU (Intel or NVIDIA), ensuring the application runs on the desired hardware backend.



== JavaScript 2D Canvas and Rendering

Finally, regarding the frontend I decided not to use a framework such as React that I already know and use, but to develop the webapp using only HTML, CSS and JavaScript so that I could learn the basics of web development, which are hidden by the abstractions provided by the modern frameworks.
Also, I did not opt for a native GUI because it cannot be easily distributed across different operating systems and architectures, while a browser is available on almost all platforms. Also, this way the rendering and the visualization can happen on different machines.
The webapp is very simple and features a sidebar with all the controls and a `<canvas>` element which provides a drawing surface where to paint the mandelbrot. For high-performance rendering, this project uses the `CanvasRenderingContext3D` API. EXPLAIN BETTER WHY

The frontend logic in `script.js` handles the visualization:
- *Binary Protocol:* The client sends coordinates as a string and receives a raw binary `ArrayBuffer` containing 16-bit iteration counts.
- *Client-Side Coloring:* To allow instant theme switching without re-requesting data, the mapping from iteration count to RGBA color happens in the browser. A `Uint32Array` palette is precomputed.
- *Buffer Management:* The received data is wrapped in a `Uint16Array`. The script iterates through this array, looks up the color in the palette, and writes the resulting 32-bit pixel value into an `ImageData` buffer's `Uint32Array` view.
- *Blitting:* The final image is pushed to the GPU-accelerated canvas using `ctx.putImageData` (or `createImageBitmap` for scaling), providing a smooth 60 FPS experience.





