#import "@preview/lilaq:0.4.0" as lq

#let rounded-box(content) = block(
  fill: none,  // Clear background
  stroke: 1pt + black,
  radius: 8pt,
  inset: 10pt,
  // For shadow effect, we need to layer boxes
  {
    // Shadow layer (positioned slightly offset)
    place(
      dx: 3pt,
      dy: 3pt,
      block(
        fill: gray.transparentize(60%),
        stroke: none,
        radius: 8pt,
        inset: 10pt,
        width: 100%,
        content
      )
    )
    // Main box
    block(
      fill: white,  // or none for truly transparent
      stroke: 1pt + black,
      radius: 8pt,
      inset: 10pt,
      content
    )
  }
)

#let pretty_code(label: none, show_line_number: true, body) = {
  // Use a 'show' rule to intercept raw blocks inside this function.
  // This works even if there are spaces or newlines around your code.
  show raw.where(block: true): it => {
    let lines = it.text.split("\n")
    
    // Create a grid: Left column = numbers, Right column = code
    grid(
      columns: (auto, 1fr),
      column-gutter: 1em,
      row-gutter: 0.6em, // Tweak this if lines look too squeezed/loose
      ..lines.enumerate().map(((i, line)) => {
        (
          if (show_line_number){align(right, text(fill: gray)[#str(i + 1)])} else {},
          // We re-wrap the line in raw() to keep basic syntax highlighting
          raw(line, lang: it.lang) 
        )
      }).flatten()
    )
  }

  // Visual container styling
  let content = rect(fill: luma(240), inset: 10pt, radius: 4pt)[ #body ]

  if label == none {
    align(center)[ #align(left)[ #content ] ]
  } else {
    figure(
      align(left)[ #content ],
      caption: label,
      supplement: [Code]
    )
  }
}

#let plot_cpu_performance(data, y_axis: left) = {
  let title = data.at("title", default: [Performance comparison]) 
  let width = data.at("width", default: 10cm)
  let height = data.at("height", default: 5cm)
  let ylim = data.at("ylim", default: 34)
  
  // Determine bounds for GPU horizontal lines
  let t_min = data.threads.first()
  let t_max = data.threads.last()

  // We will collect all plot calls in this array
  let plots = ()

  // 1. Add Ideal Scaling (kept from your snippet)
  plots.push(lq.plot((0, 14), (0,14), label: [Ideal], stroke: (paint: gray, dash: "dotted")))

  // 2. Loop through CPU benchmarks (Dynamic Lines)
  if "cpu" in data {
    for (key, y_values) in data.cpu {
      // Format label: "res_960_540_scalar" -> "960x540 scalar"
      let label_text = key.replace("res_", "").replace("_", " ").replace(" ", "x", count: 1)
      
      plots.push(lq.plot(
        data.threads, 
        y_values, 
        label: label_text
      ))
    }
  }



  // Render the diagram
  lq.diagram(
      legend: (position: top + left),
      title: title,
      width: width,
      height: height,
      xlabel: "Threads", 
      ylabel: "FPS",
      yaxis: (position: y_axis),
      xlim: (0, 14),
      ylim: (0, ylim),
      ..plots // Spread the collected plots into the function
  )
}

#let table_cpu_performance(data, tp: false) = {
  // 1. Extract keys and thread counts
  let cpu_keys = if "cpu" in data { data.cpu.keys() } else { () }
  let threads = data.threads

  // 2. Format Headers
  let header_cells = (if (tp) { [
    *Threads*
    *(#smallcaps("TP"))*
  ]} else {[ *Threads* ]},) + cpu_keys.map(key => {
    let label = key.replace("res_", "").replace("_", " ").replace(" ", "x", count: 1)
    [#label]
  })

  // 3. Pre-calculate Column Maximums
  // Map each key to its maximum value across the entire list of values
  let col_maxes = (:)
  for k in cpu_keys {
    let values = data.cpu.at(k)
    col_maxes.insert(k, calc.max(..values))
  }

  // 4. Build Table Content
  let table_content = ()
  
  // Push headers first
  table_content += header_cells

  // Loop through every thread index (row)
  for (i, t) in threads.enumerate() {
    // Column 1: The thread number
    table_content.push(str(t))

    // Iterate through keys (columns)
    for k in cpu_keys {
      let val = data.cpu.at(k).at(i)
      let max_val = col_maxes.at(k)

      // Compare current value against the pre-calculated column max
      let content = if val == max_val { strong(str(val)) } else { str(val) }
      table_content.push(content)
    }
  }

  // 5. Render the Table
  table(
    columns: 1 + cpu_keys.len(),
    align: (col, row) => center + horizon,
    fill: (col, row) => if row == 0 { gray.lighten(60%) } else { none },
    inset: 5pt,
    ..table_content
  )
}
#let plot_gpu_performance(data) = {
  let title = data.at("title", default: [GPU Performance])
  let width = 5cm
  let height = 5cm
  let ylim = 100 // Default ylim if not in data

  // 1. Parse the flat GPU data into a structured dictionary
  // Structure: ("960x540": (intel: 5.98, nvidia: 95.41), ...)
  let parsed_groups = (:)
  
  if "gpu" in data {
    for (key, value) in data.gpu {
      let parts = key.split("_") 
      // Expected key format: res_960_540_intel_gpu
      // parts: ("res", "960", "540", "intel", "gpu") or ("res", "960", "540", "nvidia", "gpu")
      
      if parts.len() >= 5 {
        let w = parts.at(1)
        let h = parts.at(2)
        let vendor = parts.at(3) // "intel" or "nvidia"
        let resolution = w + "x" + h
        
        if resolution not in parsed_groups {
          parsed_groups.insert(resolution, (:))
        }
        parsed_groups.at(resolution).insert(vendor, value)
      }
    }
  }

  // 2. Sort Resolutions by pixel width (string -> int)
  let sorted_resolutions = parsed_groups.keys().sorted(key: r => int(r.split("x").first()))

  // 3. Prepare Data for lq.bar
  let x_indices = range(sorted_resolutions.len()) // (0, 1, 2, ...)
  let intel_y = ()
  let nvidia_y = ()
  let tick_labels = ()

  for (i, res) in sorted_resolutions.enumerate() {
    let group = parsed_groups.at(res)
    intel_y.push(group.at("intel", default: 0))
    nvidia_y.push(group.at("nvidia", default: 0))
    tick_labels.push((i, [#res])) // Tuple (coordinate, label content)
  }

  // 4. Render the Diagram
  // We offset Intel to the left and Nvidia to the right to create a grouped effect.
  lq.diagram(
    title: title,
    width: width,
    height: height,
    xaxis: (
      ticks: tick_labels, // Custom labels at 0, 1, 2...
      subticks: none,     // Hide subticks for cleaner look
    ),
    // Define bounds if needed, or let lilaq auto-scale
    ylim: (0, ylim), 

    // Intel Bars (Shifted Left)
    lq.bar(
      x_indices, 
      intel_y, 
      width: 0.15,       // Bar width
      offset: -0.075,    // Shift left by half width
      fill: blue.lighten(30%), // Intel Blue
      label: "Intel GPU"
    ),

    // Nvidia Bars (Shifted Right)
    lq.bar(
      x_indices, 
      nvidia_y, 
      width: 0.15,       // Bar width
      offset: 0.075,     // Shift right by half width
      fill: green.lighten(30%), // Intel Blue
      label: "NVIDIA GPU"
    ),

    legend: (position: top + right)
  )
}

#let plot_grouped_gpu_performance(datasets, labels: none) = {
  let title = datasets.first().at("title", default: [GPU Performance Comparison])
  let width = 12cm // Increased width for multiple bars
  let height = 8cm
  let ylim = 120   // Adjust based on your max expected FPS

  // 1. Identify all unique resolutions across all datasets
  // We scan all datasets to ensure we don't miss a resolution present in only one
  let all_resolutions = ()
  for data in datasets {
    if "gpu" in data {
      for key in data.gpu.keys() {
        let parts = key.split("_")
        if parts.len() >= 5 {
          let res = parts.at(1) + "x" + parts.at(2)
          if res not in all_resolutions { all_resolutions.push(res) }
        }
      }
    }
  }
  
  // Sort resolutions by width (960 < 1920 < 3840)
  let sorted_resolutions = all_resolutions.sorted(key: r => int(r.split("x").first()))
  
  // 2. Configure Bar Dimensions
  let num_datasets = datasets.len()
  let bars_per_group = num_datasets * 2 // Intel + Nvidia per dataset
  let total_group_width = 0.8           // Occupy 80% of the space between ticks
  let single_bar_width = total_group_width / bars_per_group
  
  // 3. Prepare Plots
  let bar_plots = ()
  let x_indices = range(sorted_resolutions.len())

  // Default labels if not provided
  let bench_labels = if labels != none { labels } else { 
    range(num_datasets).map(i => "Bench " + str(i + 1)) 
  }

  for (i, data) in datasets.enumerate() {
    let dataset_label = bench_labels.at(i)
    
    // Parse values for this specific dataset
    let intel_vals = ()
    let nvidia_vals = ()
    
    for res in sorted_resolutions {
      // Reconstruct keys: res_960_540_intel_gpu
      let parts = res.split("x")
      let w = parts.at(0)
      let h = parts.at(1)
      let key_base = "res_" + w + "_" + h + "_"
      
      let i_val = if "gpu" in data { data.gpu.at(key_base + "intel_gpu", default: 0) } else { 0 }
      let n_val = if "gpu" in data { data.gpu.at(key_base + "nvidia_gpu", default: 0) } else { 0 }
      
      intel_vals.push(i_val)
      nvidia_vals.push(n_val)
    }

    // Calculate Colors (Gradient effect: older benchmarks lighter, newer darker)
    // We adjust brightness based on index 'i' relative to total datasets
    let brightness_factor = (i / num_datasets) * 40% - 20% // -20% to +20%
    let col_intel = blue.darken(brightness_factor)
    let col_nvidia = green.darken(brightness_factor)

    // Calculate Offsets
    // We want to center the group around integer x.
    // Order: [D1_Intel, D1_Nvidia, D2_Intel, D2_Nvidia ...]
    let group_start = -total_group_width / 2
    let offset_intel = group_start + (i * 2) * single_bar_width + (single_bar_width / 2)
    let offset_nvidia = group_start + (i * 2 + 1) * single_bar_width + (single_bar_width / 2)

    // Add Intel Bar for this Dataset
    bar_plots.push(lq.bar(
      x_indices,
      intel_vals,
      width: single_bar_width,
      offset: offset_intel,
      fill: col_intel,
      label: dataset_label + " (Intel)"
    ))

    // Add Nvidia Bar for this Dataset
    bar_plots.push(lq.bar(
      x_indices,
      nvidia_vals,
      width: single_bar_width,
      offset: offset_nvidia,
      fill: col_nvidia,
      label: dataset_label + " (Nvidia)"
    ))
  }

  // 4. Render Diagram
  lq.diagram(
    width: width,
    height: height,
    ylabel: "FPS",
    ylim: (0,100),
    xaxis: (
      ticks: sorted_resolutions.enumerate().map(p => (p.at(0), [ #p.at(1) ])),
      subticks: none
    ),
    legend: (
      position: top + right
    ),
    ..bar_plots
  )
}
#let plot_cpu_performance(data, y_axis: left) = {
  let title = data.at("title", default: [Speedup comparison]) 
  let width = data.at("width", default: 10cm)
  let height = data.at("height", default: 5cm)
  let ylim = 14 // data.at("ylim", default: 14) // Adjusted default for Speedup (approx max threads)
  
  // 1. Slice threads to remove the first element (baseline)
  let t_subset = data.threads.slice(1)
  
  let plots = ()

  // 2. Add Ideal Scaling (Linear y = x)
  plots.push(lq.plot((0, 14), (0,14), label: [Ideal], stroke: (paint: gray, dash: "dotted")))

  // 3. Loop through CPU benchmarks
  if "cpu" in data {
    for (key, y_values) in data.cpu {
      // Format label
      let parts = key.split("_")
      let label_text = parts.at(2) + "p " + smallcaps(parts.at(3))
      
      // LOGIC CHANGE:
      // 1. Get baseline (first element)
      let baseline = y_values.first()
      
      // 2. Remove first element from values and divide the rest by baseline
      let speedup_values = y_values.slice(1).map(v => v / baseline)
      
      plots.push(lq.plot(
        t_subset, 
        speedup_values, 
        label: label_text
      ))
    }
  }

  // Render the diagram
  lq.diagram(
      legend: (position: top + left),
      title: title,
      width: width,
      height: height,
      xlabel: "Threads", 
      ylabel: "Speedup (x)", // Changed label
      yaxis: (position: y_axis),
      xlim: (0, 14),
      ylim: (0, ylim),
      ..plots 
  )
}

#let table_cpu_performance(data, tp: false) = {
  // 1. Extract keys and thread counts
  let cpu_keys = if "cpu" in data { data.cpu.keys() } else { () }
  
  // LOGIC CHANGE: Slice threads to skip the first one
  let threads = data.threads.slice(1)

  // 2. Format Headers
  let header_cells = (if (tp) { [
    *Threads*
    *(#smallcaps("TP"))*
  ]} else {[ *Threads* ]},) + cpu_keys.map(key => {
      let parts = key.split("_")
      let label = parts.at(2) + "p " + smallcaps(parts.at(3))
    [#label]
  })

  // 3. Pre-calculate Column Maximums (based on Speedup now)
  let col_maxes = (:)
  for k in cpu_keys {
    let raw_values = data.cpu.at(k)
    let baseline = raw_values.first()
    // Calculate speedups for the slice to find max
    let speedups = raw_values.slice(1).map(v => v / baseline)
    col_maxes.insert(k, calc.max(..speedups))
  }

  // 4. Build Table Content
  let table_content = ()
  
  table_content += header_cells

  // Loop through the SLICED threads
  // Note: we enumerate the sliced array, so 'i' starts at 0, 
  // but we need to access the original data at 'i + 1'
  for (i, t) in threads.enumerate() {
    
    // Column 1: The thread number
    table_content.push(str(t))

    // Iterate through keys (columns)
    for k in cpu_keys {
      let raw_vals = data.cpu.at(k)
      
      // Calculate Speedup: current_val / baseline_val
      // current_val is at index i + 1 (because we skipped the first row)
      let val = raw_vals.at(i + 1) / raw_vals.first()
      
      // Round for display clarity
      let display_val = calc.round(val, digits: 2)
      
      let max_val = col_maxes.at(k)

      // Compare
      let content = if val == max_val { strong(str(display_val) + "x") } else { str(display_val) + "x" }
      table_content.push(content)
    }
  }

  // 5. Render the Table
  table(
    columns: (5.5em,) * (1 + cpu_keys.len()),
    align: (col, row) => center + horizon,
    fill: (col, row) => if row == 0 { gray.lighten(60%) } else { none },
    inset: 5pt,
    ..table_content
  )
}

#let plot_cpu_performance_normalized(data, y_axis: left) = {
  let title = data.at("title", default: [Speedup comparison]) 
  let width = data.at("width", default: 10cm)
  let height = data.at("height", default: 5cm)
  let ylim = 56
  
  // 1. Slice threads to remove the first element (baseline)
  let t_subset = data.threads.slice(1)
  
  let plots = ()

  // 2. Add Ideal Scaling (Linear y = x)
  plots.push(lq.plot((0, 14), (0,14), label: [Ideal], stroke: (paint: gray, dash: "dotted")))
  plots.push(lq.plot((0, 14), (0,56), label: [Ideal SIMD], stroke: (paint: gray, dash: "dotted")))

  // 3. Loop through CPU benchmarks
  if "cpu" in data {
    for (key, y_values) in data.cpu {
      // LOGIC CHANGE: Format label to "540p scalar" etc.
      // key format: res_WIDTH_HEIGHT_TYPE (e.g., res_960_540_scalar)
      let parts = key.split("_")
      let label_text = parts.at(2) + "p " + smallcaps(parts.at(3))
      
      // 1. Get baseline: Always use the SCALAR value at index 0 for this resolution
      let scalar_key = key.replace("simd", "scalar")
      let baseline = data.cpu.at(scalar_key).first()
      
      // 2. Remove first element from values and divide the rest by the scalar baseline
      let speedup_values = y_values.slice(1).map(v => v / baseline)
      
      plots.push(lq.plot(
        t_subset, 
        speedup_values, 
        label: label_text
      ))
    }
  }

  // Render the diagram
  lq.diagram(
      legend: (position: top + left),
      title: title,
      width: width,
      height: height,
      xlabel: "Threads", 
      ylabel: "Speedup (x)", 
      yaxis: (position: y_axis),
      xlim: (0, 14),
      ylim: (0, ylim),
      ..plots 
  )
}

#let table_cpu_performance_normalized(data, tp: false) = {
  // 1. Extract keys and thread counts
  let cpu_keys = if "cpu" in data { data.cpu.keys() } else { () }
  
  // Slice threads to skip the first one
  let threads = data.threads.slice(1)

  // 2. Format Headers
  // LOGIC CHANGE: Format labels to "540p scalar" etc.
  let header_cells = (if (tp) { [
    *Threads*
    *(#smallcaps("TP"))*
  ]} else {[ *Threads* ]},) + cpu_keys.map(key => {
    let parts = key.split("_")
    let label = parts.at(2) + "p " + smallcaps(parts.at(3))
    [#label]
  })


  // 3. Pre-calculate Column Maximums (based on Scalar Baseline Speedup)
  let col_maxes = (:)
  for k in cpu_keys {
    let raw_values = data.cpu.at(k)
    
    // Baseline logic
    let scalar_key = k.replace("simd", "scalar")
    let baseline = data.cpu.at(scalar_key).first()
    
    // Calculate speedups for the slice to find max
    let speedups = raw_values.slice(1).map(v => v / baseline)
    col_maxes.insert(k, calc.max(..speedups))
  }

  // 4. Build Table Content
  let table_content = ()
  
  table_content += header_cells

  // Loop through the SLICED threads
  for (i, t) in threads.enumerate() {
    
    // Column 1: The thread number
    table_content.push(str(t))

    // Iterate through keys (columns)
    for k in cpu_keys {
      let raw_vals = data.cpu.at(k)
      
      // Baseline logic
      let scalar_key = k.replace("simd", "scalar")
      let baseline = data.cpu.at(scalar_key).first()
      
      // Calculate Speedup: current_val / scalar_baseline
      let val = raw_vals.at(i + 1) / baseline
      
      // Round for display clarity
      let display_val = calc.round(val, digits: 2)
      
      let max_val = col_maxes.at(k)

      // Compare
      let content = if val == max_val { strong(str(display_val) + "x") } else { str(display_val) + "x" }
      table_content.push(content)
    }
  }

  // 5. Render the Table
  table(
    columns: (5.5em,) * (1 + cpu_keys.len()),
    align: (col, row) => center + horizon,
    fill: (col, row) => if row == 0 { gray.lighten(60%) } else { none },
    inset: 5pt,
    ..table_content
  )
}

#let plot_simd_vs_scalar_efficiency(data, y_axis: left) = {
  let title = data.at("title", default: [SIMD Efficiency (Ratio)]) 
  let width = data.at("width", default: 10cm)
  let height = data.at("height", default: 5cm)
  let ylim = 6 // SIMD usually provides 4x-8x speedup, 6 is a safe ceiling for doubles
  
  // 1. Slice threads to remove the first element
  let t_subset = data.threads.slice(1)
  
  let plots = ()

  plots.push(lq.plot((0, 14), (4,4), label: [Ideal], stroke: (paint: gray, thickness: 1.5pt ,dash: "dashed")))

  // 2. Loop through CPU benchmarks to find pairs
  if "cpu" in data {
    // We need to identify unique resolutions first to process pairs
    let processed_resolutions = ()

    for (key, val) in data.cpu {
      if "scalar" in key {
         let scalar_key = key
         let simd_key = key.replace("scalar", "simd")
         
         // Ensure both exist
         if simd_key in data.cpu {
            let parts = key.split("_")
            // Label: "540p" (Since this line represents the relationship between SIMD/Scalar)
            let label_text = parts.at(2) + "p"
            
            let scalar_values = data.cpu.at(scalar_key)
            let simd_values = data.cpu.at(simd_key)

            // Logic: Calculate Ratio (SIMD / Scalar) for the sliced subset
            // Math: (SIMD_Norm / Scalar_Norm) simplifies to (SIMD_Raw / Scalar_Raw)
            let ratio_values = ()
            
            // Iterate through the sliced indices (1 to end)
            for i in range(1, scalar_values.len()) {
               let s = scalar_values.at(i)
               let v = simd_values.at(i)
               // Avoid division by zero if benchmark failed
               if s != 0 { ratio_values.push(v / s) } else { ratio_values.push(0) }
            }

            plots.push(lq.plot(
              t_subset, 
              ratio_values, 
              label: label_text
            ))
         }
      }
    }
  }

  // Render the diagram
  lq.diagram(
      legend: (position: top + left),
      title: title,
      width: width,
      height: height,
      xlabel: "Threads", 
      ylabel: "Efficiency Ratio (x)", 
      yaxis: (position: y_axis),
      xlim: (0, 14),
      ylim: (0, ylim),
      ..plots 
  )
}

#let table_simd_vs_scalar_efficiency(data, tp: false) = {
  // 1. Identify Resolution Keys (only unique resolutions)
  let resolutions = ()
  if "cpu" in data {
    for k in data.cpu.keys() {
      if "scalar" in k {
        resolutions.push(k) // Keep the full scalar key as the identifier
      }
    }
  }
  
  // Slice threads to skip the first one
  let threads = data.threads.slice(1)

  // 2. Format Headers
  let header_cells = (if (tp) { [
    *Threads*
    *(#smallcaps("TP"))*
  ]} else {[ *Threads* ]},) + resolutions.map(key => {
    let parts = key.split("_")
    let label = parts.at(2) + "p"
    [#label]
  })

  // 3. Pre-calculate Column Maximums
  let col_maxes = (:)
  for k in resolutions {
    let scalar_vals = data.cpu.at(k)
    let simd_vals = data.cpu.at(k.replace("scalar", "simd"))
    
    let ratios = ()
    for i in range(1, scalar_vals.len()) {
       ratios.push(simd_vals.at(i) / scalar_vals.at(i))
    }
    col_maxes.insert(k, calc.max(..ratios))
  }

  // 4. Build Table Content
  let table_content = ()
  
  table_content += header_cells

  // Loop through the SLICED threads
  for (i, t) in threads.enumerate() {
    
    // Column 1: The thread number
    table_content.push(str(t))

    // Iterate through resolutions (columns)
    for k in resolutions {
      let scalar_vals = data.cpu.at(k)
      let simd_vals = data.cpu.at(k.replace("scalar", "simd"))
      
      // Calculate Ratio at index i+1
      let val = simd_vals.at(i + 1) / scalar_vals.at(i + 1)
      
      let display_val = calc.round(val, digits: 2)
      let max_val = col_maxes.at(k)

      // Compare
      let content = if val == max_val { strong(str(display_val) + "x") } else { str(display_val) + "x" }
      table_content.push(content)
    }
  }

  // 5. Render the Table
  table(
    columns: (7em,) * (1 + resolutions.len()),
    align: (col, row) => center + horizon,
    fill: (col, row) => if row == 0 { gray.lighten(60%) } else { none },
    inset: 5pt,
    ..table_content
  )
}
