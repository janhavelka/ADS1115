#!/usr/bin/env python3
"""
Generate Version.h from library.json version field.

This script is run as a PlatformIO pre-build script to ensure
Version.h always matches library.json.

Usage in platformio.ini:
    extra_scripts = pre:scripts/generate_version.py
"""

import json
import os
import re
from pathlib import Path

Import("env")  # PlatformIO environment

def generate_version_header():
    """Generate Version.h from library.json"""
    
    # Find project root (where library.json is)
    project_dir = Path(env.get("PROJECT_DIR", "."))
    library_json = project_dir / "library.json"
    
    if not library_json.exists():
        print(f"Warning: library.json not found at {library_json}")
        return
    
    # Read version from library.json
    with open(library_json, "r") as f:
        lib_data = json.load(f)
    
    version = lib_data.get("version", "0.0.0")
    name = lib_data.get("name", "DEVICE")
    
    # Parse version components
    match = re.match(r"(\d+)\.(\d+)\.(\d+)", version)
    if not match:
        print(f"Warning: Invalid version format: {version}")
        major, minor, patch = 0, 0, 0
    else:
        major, minor, patch = match.groups()
    
    # Determine namespace/device name from library name
    namespace = name.upper()
    
    # Generate header content
    header_content = f'''/// @file Version.h
/// @brief Auto-generated version information
/// @warning DO NOT EDIT - Generated from library.json by generate_version.py
#pragma once

namespace {name} {{

/// Library version string
static constexpr const char* VERSION = "{version}";

/// Version components
static constexpr int VERSION_MAJOR = {major};
static constexpr int VERSION_MINOR = {minor};
static constexpr int VERSION_PATCH = {patch};

/// Version as single integer (major * 10000 + minor * 100 + patch)
static constexpr int VERSION_INT = {int(major) * 10000 + int(minor) * 100 + int(patch)};

}} // namespace {name}
'''
    
    # Find output path
    include_dir = project_dir / "include" / name
    version_header = include_dir / "Version.h"
    
    # Ensure directory exists
    include_dir.mkdir(parents=True, exist_ok=True)
    
    # Check if update needed
    if version_header.exists():
        with open(version_header, "r") as f:
            existing = f.read()
        if existing == header_content:
            print(f"Version.h up to date (v{version})")
            return
    
    # Write new header
    with open(version_header, "w") as f:
        f.write(header_content)
    
    print(f"Generated Version.h (v{version})")

# Run on pre-build
generate_version_header()
