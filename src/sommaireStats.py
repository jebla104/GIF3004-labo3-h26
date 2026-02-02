#!/usr/bin/env python3
"""
Script to compute statistics (average, median, std.dev., 5% and 95% percentiles)
for moy and max values from performance log files.

Input format:
[timestamp] Entree 1: moy=16.5 fps, max=128.3 ms | Entree 2: moy=16.7 fps, max=131.3 ms | ...
"""

import sys
import re
import numpy as np
from collections import defaultdict


def parse_line(line):
    """
    Parse a single line and extract moy and max values for each entree.
    Returns a dict: {entree_num: {'moy': float, 'max': float}}
    """
    results = {}
    
    # Pattern to match each entree block
    entree_pattern = r'Entree\s+(\d+):\s+moy=([0-9.]+)\s+fps,\s+max=([0-9.]+)\s+ms'
    
    matches = re.findall(entree_pattern, line)
    for match in matches:
        entree_num = int(match[0])
        moy_val = float(match[1])
        max_val = float(match[2])
        results[entree_num] = {'moy': moy_val, 'max': max_val}
    
    return results


def compute_statistics(values):
    """
    Compute statistics for a list of values.
    Returns: (average, median, std_dev, percentile_5, percentile_95)
    """
    if not values:
        return None
    
    arr = np.array(values)
    return {
        'average': np.mean(arr),
        'median': np.median(arr),
        'std_dev': np.std(arr),
        'percentile_5': np.percentile(arr, 5),
        'percentile_95': np.percentile(arr, 95)
    }


def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <input_file>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    # Store all values for each entree
    # {entree_num: {'moy': [values], 'max': [values]}}
    data = defaultdict(lambda: {'moy': [], 'max': []})
    
    try:
        with open(input_file, 'r') as f:
            # Read all lines and filter out empty ones
            lines = [line.strip() for line in f if line.strip()]
        
        # Exclude first and last lines
        if len(lines) > 2:
            lines = lines[1:-1]
        else:
            print("Warning: Not enough data lines (need at least 3 to exclude first and last).")
            if len(lines) == 0:
                print("No data found in the input file.")
                sys.exit(1)
        
        for line in lines:
            parsed = parse_line(line)
            for entree_num, values in parsed.items():
                data[entree_num]['moy'].append(values['moy'])
                data[entree_num]['max'].append(values['max'])
    except FileNotFoundError:
        print(f"Error: File '{input_file}' not found.")
        sys.exit(1)
    except Exception as e:
        print(f"Error reading file: {e}")
        sys.exit(1)
    
    if not data:
        print("No data found in the input file.")
        sys.exit(1)
    
    # Sort entrees by number
    sorted_entrees = sorted(data.keys())
    
    # Output statistics for each entree
    print("=" * 80)
    print("Performance Statistics")
    print("=" * 80)
    
    for entree_num in sorted_entrees:
        entree_data = data[entree_num]
        n_samples = len(entree_data['moy'])
        
        print(f"\nEntree {entree_num} ({n_samples} samples)")
        print("-" * 40)
        
        # Statistics for moy (fps)
        moy_stats = compute_statistics(entree_data['moy'])
        if moy_stats:
            print(f"  moy (fps):")
            print(f"    Average:        {moy_stats['average']:.2f}")
            print(f"    Median:         {moy_stats['median']:.2f}")
            print(f"    Std. Dev.:      {moy_stats['std_dev']:.2f}")
            print(f"    5% Percentile:  {moy_stats['percentile_5']:.2f}")
            print(f"    95% Percentile: {moy_stats['percentile_95']:.2f}")
        
        # Statistics for max (ms)
        max_stats = compute_statistics(entree_data['max'])
        if max_stats:
            print(f"  max (ms):")
            print(f"    Average:        {max_stats['average']:.2f}")
            print(f"    Median:         {max_stats['median']:.2f}")
            print(f"    Std. Dev.:      {max_stats['std_dev']:.2f}")
            print(f"    5% Percentile:  {max_stats['percentile_5']:.2f}")
            print(f"    95% Percentile: {max_stats['percentile_95']:.2f}")
    
    print("\n" + "=" * 80)


if __name__ == "__main__":
    main()
