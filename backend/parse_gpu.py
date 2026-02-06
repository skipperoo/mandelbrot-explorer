import sys
import re
from collections import defaultdict

def parse_and_average(file_path):
    # Dictionary to store lists of FPS values
    data_store = defaultdict(list)
    current_res = None
    
    try:
        with open(file_path, 'r') as f:
            for line in f:
                line = line.strip()
                
                # 1. Detect Resolution Header
                # Matches: ######### GPU Benchmark (960x540) #########
                res_match = re.search(r"GPU Benchmark \((\d+)x(\d+)\)", line)
                if res_match:
                    width = res_match.group(1)
                    height = res_match.group(2)
                    current_res = f"res_{width}_{height}"
                    continue

                # 2. Detect FPS Lines
                if current_res and "FPS:" in line:
                    fps_match = re.search(r"FPS:\s*([\d\.]+)", line)
                    if fps_match:
                        fps_val = float(fps_match.group(1))
                        
                        if "[Intel GPU]" in line:
                            key = f"{current_res}_intel_gpu"
                            data_store[key].append(fps_val)
                        elif "[NVIDIA GPU]" in line:
                            key = f"{current_res}_nvidia_gpu"
                            data_store[key].append(fps_val)
                            
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' was not found.")
        sys.exit(1)
    except Exception as e:
        print(f"An error occurred: {e}")
        sys.exit(1)

    # 3. Output in Typst Dictionary Format
    print("gpu: (")
    
    # We define a specific order to keep the output clean and sorted
    # Get all unique resolution keys found in the data
    found_keys = data_store.keys()
    
    # Helper to sort keys: 960... < 1920... < 3840...
    def sort_key(k):
        # Extract the width (first number after "res_")
        match = re.search(r"res_(\d+)_", k)
        width = int(match.group(1)) if match else 0
        # Sort by width, then by vendor (Intel before Nvidia alphabetically)
        return (width, k)

    for key in sorted(found_keys, key=sort_key):
        values = data_store[key]
        avg = sum(values) / len(values)
        print(f"  {key}: {avg:.2f},")
                
    print("),")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <path_to_log_file>")
        sys.exit(1)
        
    log_file_path = sys.argv[1]
    parse_and_average(log_file_path)
