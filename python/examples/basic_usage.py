#!/usr/bin/env python3
"""
Basic usage example of Lv-00 Python bindings.
"""

import sys
sys.path.insert(0, '../..')

from lv00 import Graph, SymbolicCoord
from fractions import Fraction


def main():
    print("Lv-00 Python Bindings - Basic Usage Example")
    print("=" * 50)
    
    # Example 1: Create symbolic coordinates
    print("\n1. Symbolic Coordinates:")
    c1 = SymbolicCoord(3)  # Integer
    c2 = SymbolicCoord(Fraction(1, 2))  # Fraction
    c3 = SymbolicCoord("3/4")  # From string
    
    print(f"   c1 = {c1}")
    print(f"   c2 = {c2}")
    print(f"   c3 = {c3}")
    
    # Arithmetic
    print("\n2. Coordinate Arithmetic:")
    sum_coord = c1 + c2
    diff_coord = c1 - c2
    prod_coord = c1 * c2
    
    print(f"   {c1} + {c2} = {sum_coord}")
    print(f"   {c1} - {c2} = {diff_coord}")
    print(f"   {c1} * {c2} = {prod_coord}")
    
    # Example 2: Create a graph with points
    print("\n3. Creating a Graph:")
    g = Graph()
    print(f"   Created: {g}")
    
    # Add points
    p1 = g.add_point(0, 0)
    p2 = g.add_point(3, 4)
    p3 = g.add_point(Fraction(1, 2), Fraction(3, 4))
    
    print(f"   Added points:")
    print(f"     p1 = {p1}")
    print(f"     p2 = {p2}")
    print(f"     p3 = {p3}")
    print(f"   Graph now: {g}")
    
    # Add line segment
    print("\n4. Adding Line Segments:")
    seg1 = g.add_line_segment(p1, p2)
    print(f"   {seg1}")
    print(f"   Graph now: {g}")
    
    # Normalize
    print("\n5. Normalization:")
    result = g.normalize()
    print(f"   {result}")
    
    print("\n" + "=" * 50)
    print("Example completed successfully!")


if __name__ == "__main__":
    main()
