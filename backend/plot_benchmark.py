import re
import sys
import pandas as pd
import plotly.express as px
import plotly.graph_objects as go

data = []
gpu_data = []
current_res = None
current_threads = None

if len(sys.argv) != 2:
    print(f"Usage: python {sys.argv[0]} /path/to/log")
    exit(1)

with open(sys.argv[1], 'r') as f:
    lines = f.readlines()

for line in lines:
    line = line.strip()
    
    # Parse Header: Benchmark (960x540) with 2 threads
    header_match = re.search(r"Benchmark \((.*?)\) with (\d+) threads", line)
    if header_match:
        current_res = header_match.group(1)
        current_threads = int(header_match.group(2))
        continue

    # Parse GPU Header: GPU Benchmark (960x540)
    gpu_header_match = re.search(r"GPU Benchmark \((.*?)\)", line)
    if gpu_header_match:
        current_res = gpu_header_match.group(1)
        current_threads = None
        continue
    
    # Parse Data: [Scalar] Time: 0.0567 seconds | FPS: 17.64
    data_match = re.search(r"\[(.*?)\] Time: ([\d\.]+) seconds \| FPS: ([\d\.]+)", line)
    if data_match and current_res:
        algo_type = data_match.group(1)
        time_val = float(data_match.group(2))
        fps_val = float(data_match.group(3))
        
        if current_threads is not None:
            data.append({
                "Resolution": current_res,
                "Threads": current_threads,
                "Type": algo_type,
                "Time": time_val,
                "FPS": fps_val
            })
        else:
            gpu_data.append({
                "Resolution": current_res,
                "Type": algo_type,
                "Time": time_val,
                "FPS": fps_val
            })

df = pd.DataFrame(data)

# Create a combined label for color differentiation
df["Label"] = df["Resolution"] + " - " + df["Type"]

# Define resolution order for consistent legend ordering
resolution_order = ["960x540", "1920x1080", "3840x2160"]
label_order = []
for res in resolution_order:
    label_order.append(f"{res} - Scalar")
    label_order.append(f"{res} - SIMD")
    label_order.append(f"{res} - Intel GPU")
    label_order.append(f"{res} - NVIDIA GPU")

# Create the plot with all lines in one graph
fig = px.line(
    df,
    x="Threads",
    y="FPS",
    color="Label",
    markers=True,
    title="Benchmark Performance: CPU vs GPU",
    category_orders={"Label": label_order},
    hover_data=["Resolution", "Type", "Time"]
)

# Add GPU benchmarks as horizontal lines
thread_range = [df["Threads"].min(), df["Threads"].max()]

# Sort GPU data by resolution and type to match label_order
gpu_data_sorted = sorted(gpu_data, key=lambda x: (resolution_order.index(x["Resolution"]), x["Type"]))

for gpu_entry in gpu_data_sorted:
    res = gpu_entry["Resolution"]
    algo = gpu_entry["Type"]
    fps = gpu_entry["FPS"]
    label = f"{res} - {algo}"
    
    fig.add_trace(go.Scatter(
        x=thread_range,
        y=[fps, fps],
        mode='lines',
        name=label,
        line=dict(dash='dot', width=2),
        legendgroup=label,
        hovertemplate=f"<b>{label}</b><br>FPS: {fps}<br>Time: {gpu_entry['Time']}s<extra></extra>"
    ))

# Add single ideal scaling line (y=x)
fig.add_trace(go.Scatter(
    x=thread_range,
    y=thread_range,
    mode='lines',
    name='Ideal Scaling (y=x)',
    line=dict(dash='dash', width=2, color='grey'),
    opacity=0.5,
    showlegend=True,
    hovertemplate="Ideal Scaling<br>Threads: %{x}<br>FPS: %{y}<extra></extra>"
))

# Apply dark theme and improve layout
fig.update_layout(
    # template="plotly_dark",
    xaxis_title="Thread Count",
    yaxis_title="FPS (Higher is Better)",
    hovermode="x unified",
    legend_title_text="Configuration"
)

fig.show()
fig.write_html(f"{sys.argv[1][:sys.argv[1].index(".")]}.html")
