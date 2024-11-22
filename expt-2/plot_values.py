import os
import numpy as np
import matplotlib.pyplot as plt
from collections import deque
from scipy.stats import linregress

def moving_average(lst, window_size=1000):
    """Calculate moving average of a list with a given window size."""
    if len(lst) < window_size:
        return []  # Return empty list if less than window size
    
    averages = []
    window = deque(maxlen=window_size)  # Keeps the last `window_size` elements

    for num in lst:
        window.append(num)
        if len(window) == window_size:
            averages.append(sum(window) / window_size)

    return averages


def read_file(file_path):
    # Initialize three lists for INSERT, READ, and REMOVE operations
    insert_list = []
    read_list = []
    remove_list = []
    
    # Read the file line by line
    with open(file_path, 'r') as file:
        for line in file:
            # Split the line into two numbers
            try: operation, number = map(int, line.split())
            except: continue
            
            # Append the number to the appropriate list based on the operation
            if operation == 0:
                insert_list.append(number)
            elif operation == 1:
                read_list.append(number)
            elif operation == 2:
                remove_list.append(number)
    
    # Remove the first 300 elements from each list if they have more than 300 elements
    insert_list = insert_list[300:-1] if len(insert_list) > 300 else []
    read_list = read_list[300:-1] if len(read_list) > 300 else []
    remove_list = remove_list[300:-1] if len(remove_list) > 300 else []
    
    # Calculate the moving averages
    insert_list = moving_average(insert_list)
    read_list = moving_average(read_list)
    remove_list = moving_average(remove_list)

    return insert_list, read_list, remove_list


def plot_distribution(all_insert_data, all_read_data, all_remove_data, output_dir="."):
    """
    Plots the distribution of insert, read, and remove data with different colors for each num_hash.
    Generates separate plots for INSERT, READ, and REMOVE operations.
    
    :param all_insert_data: List of tuples where each tuple is (num_hash, insert_data)
    :param all_read_data: List of tuples where each tuple is (num_hash, read_data)
    :param all_remove_data: List of tuples where each tuple is (num_hash, remove_data)
    :param output_dir: Directory where the plots will be saved (default: "plots")
    """
    # Create directory if it does not exist
    import os
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Function to calculate and plot percentiles for INSERT
    def process_insert_data(data, operation_name, output_file):
        fig, ax = plt.subplots(figsize=(14, 10))

        for num_hash, insert_data in data:
            ax.plot(
                range(len(insert_data)),
                insert_data,
                label=f'{operation_name} (num_hash={num_hash})',
                linestyle='-', 
                marker='o', 
                markersize=1  # Make points smaller
            )

            # Calculate and print percentiles
            percentiles = np.percentile(insert_data, [1, 5, 25, 50, 75, 95, 99])
            print(f"Percentiles for {operation_name} (num_hash={num_hash}):")
            print(f"1%: {percentiles[0]:.2f} \n5%: {percentiles[1]:.2f} \n"
                  f"25%: {percentiles[2]:.2f} \n50%: {percentiles[3]:.2f} \n"
                  f"75%: {percentiles[4]:.2f} \n95%: {percentiles[5]:.2f} \n99%: {percentiles[6]:.2f}\n")
            print("\n")


        ax.set_ylabel('Time')
        ax.set_title(f'INSERT Data by num_hash')
        ax.legend(loc='upper right', fontsize=9)

        # Save the figure to the output directory
        plt.savefig(output_file, bbox_inches='tight')
        plt.close()


    # Function to perform linear regression and plot
    def process_read_remove_data(data, operation_name, output_file):
        fig, ax = plt.subplots(figsize=(14, 10))

        for num_hash, operation_data in data:
            # Plot original data
            ax.plot(
                range(len(operation_data)),
                operation_data,
                label=f'{operation_name} (num_hash={num_hash})',
                linestyle='-', 
                marker='o', 
                markersize=1  # Make points smaller
            )

            # Perform linear regression
            x = np.arange(len(operation_data))
            slope, intercept, _, _, _ = linregress(x, operation_data)
            print(f"Slope of regression line for {operation_name} (num_hash={num_hash}): {slope:.4f}")

            # Plot regression line
            regression_line = slope * x + intercept
            ax.plot(x, regression_line, label=f'Regression Line (num_hash={num_hash})', linestyle='--')

        ax.set_xlabel('Index')
        ax.set_ylabel('Data Value')
        ax.set_title(f'{operation_name} Data by num_hash')
        ax.legend(loc='upper right', fontsize=9)

        # Save the figure to the output directory
        plt.savefig(output_file, bbox_inches='tight')
        plt.close()


    # Plot and save the INSERT data
    process_insert_data(all_insert_data, "INSERT", os.path.join(output_dir, "insert_distribution.png"))
    print("\n")

    # Plot and save the READ data
    process_read_remove_data(all_read_data, "READ", os.path.join(output_dir, "read_distribution.png"))
    print("\n")

    # Plot and save the REMOVE data
    process_read_remove_data(all_remove_data, "REMOVE", os.path.join(output_dir, "remove_distribution.png"))
    print("\n")


def process_data(data_list):
    pass

def process_files(file_patterns):
    """Processes the files and prints percentiles and plots the distribution."""
    all_insert_data = []
    all_read_data = []
    all_remove_data = []

    for num_hash in file_patterns:
        file_name = f'outputs/output_client_{num_hash}.txt'
        
        if os.path.exists(file_name):
            insert_list, read_list, remove_list = read_file(file_name)
        else:
            print(f"File {file_name} does not exist.")

        all_insert_data.append((num_hash, insert_list))
        all_read_data.append((num_hash, read_list))
        all_remove_data.append((num_hash, remove_list))
    
    # Plot the distribution for all files
    plot_distribution(all_insert_data, all_read_data, all_remove_data, output_dir=".")

# Define the range of NUM_THREADS (from 1 to 24)
hash_bin_range = [10, 30, 100, 300, 1000, 3000, 10000, 30000, 100000]  # Stepping by 4 (1, 5, 9, ...)

# Process the files for the specified range of NUM_THREADS
process_files(hash_bin_range)
