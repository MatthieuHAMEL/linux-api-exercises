#!/usr/bin/perl
use strict;
use warnings;

# personal build script that takes a recent gcc&cmake ...

my $proj_path = "/home/matou/Documents/MatHub/kerrisk";
$ENV{PATH} = $proj_path . "/cmake-4.1.2-linux-x86_64/bin:$ENV{PATH}";
$ENV{PATH} = $proj_path . "/opt/gcc-15.2/bin:$ENV{PATH}";
$ENV{LANG} = "en"; # I want compiler messages in english


my $build_type;
if (@ARGV) {
  if ($ARGV[0] eq '-g') {
    $build_type = 'Debug';
  }
  elsif ($ARGV[0] eq '-r') {
    $build_type = 'Release';
  }
  else {
    die "Usage: $0 [-g | -r]\n".
        "  -g : configure + build in Debug\n".
        "  -r : configure + build in Release\n".
        "  (no option): build using existing configuration\n";
  }
}

my $build_dir = "$proj_path/build";
my $configure_cmd = "cmake -G Ninja -S \"$proj_path\" -B \"$build_dir\"";

# Only override CMAKE_BUILD_TYPE when -g or -r is used
if (defined $build_type) {
  $configure_cmd .= " -DCMAKE_BUILD_TYPE=$build_type";
}

# 1. Configure
my $ret = system($configure_cmd);
if ($ret != 0) {
  die "CMake configuration failed (exit code ".($ret >> 8).")\n";
}

# 2. Build
$ret = system("cmake --build \"$build_dir\"");
if ($ret != 0) {
  die "Build failed (exit code ".($ret >> 8).")\n";
}

exit 0;
