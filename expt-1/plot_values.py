import os
import numpy as np
import matplotlib.pyplot as plt

def read_file(file_path):
    """Reads numbers from a file and returns them as a list."""
    with open(file_path, 'r') as f:
        numbers = [int(line.strip()) for line in f.readlines()]
        numbers = numbers[:-1]
    return numbers

def calculate_percentiles(data):
    """Calculates and returns requested percentiles."""
    percentiles = {
        '1%': np.percentile(data, 1),
        '5%': np.percentile(data, 5),
        '25%': np.percentile(data, 25),
        '50%': np.percentile(data, 50),
        '75%': np.percentile(data, 75),
        '95%': np.percentile(data, 95),
        '99%': np.percentile(data, 99)
    }
    return percentiles

def plot_distribution(all_data):

    """Plots the distribution of numbers for all files."""
    plt.figure(figsize=(14, 10))

    min_limit = 0
    max_limit = 5000
    # for data, num_threads in all_data:
    #     min_val = np.percentile(data, 1)
    #     max_val = np.percentile(data, 99)
    #     if min_val < min_limit: min_limit = min_val
    #     if max_val > max_limit: max_limit = max_val
    bins = np.linspace(min_limit, max_limit, 501)
    
    for data, num_threads in all_data:
        plt.hist(data, bins=bins, alpha=0.5, label=f'Threads {num_threads}')
    
    plt.yscale('log')
    plt.xlabel('time nsec')
    plt.ylabel('Frequency')
    plt.title('Request time distribution')
    plt.legend()
    plt.grid(True)
    plt.savefig("request_time.png")

def process_files(file_patterns):
    """Processes the files and prints percentiles and plots the distribution."""
    all_data = []

    for num_threads in file_patterns:
        file_name = f'outputs/output_client_{num_threads}.txt'
        
        if os.path.exists(file_name):
            data = read_file(file_name)
            percentiles = calculate_percentiles(data)
            
            print(f"\nPercentiles Request time NUM_THREADS = {num_threads}:")
            for p, value in percentiles.items():
                print(f"{p}: {value}")
            
            all_data.append((data, num_threads))
        else:
            print(f"File {file_name} does not exist.")
    
    # Plot the distribution for all files
    plot_distribution(all_data)

# Define the range of NUM_THREADS (from 1 to 24)
num_threads_range = [1, 3, 5, 9, 13, 17, 21, 25]  # Stepping by 4 (1, 5, 9, ...)

# Process the files for the specified range of NUM_THREADS
process_files(num_threads_range)