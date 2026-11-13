#!/usr/bin/env bash
# Build the unmodified Android 7.3 arm64 GKI. This does not produce an avr port.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:?Usage: build_baseline.sh /absolute/path/to/new-build-directory}
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
    echo 'The official prebuilt toolchain requires a Linux x86_64 build host.' >&2
    exit 2
fi
for required in repo git python3; do
    command -v "$required" >/dev/null || { echo "Missing tool: $required" >&2; exit 2; }
done
if [[ "$build_dir" != /* ]]; then
    echo 'Build directory must be an absolute path.' >&2
    exit 2
fi
if [[ -e "$build_dir" ]]; then
    echo 'Choose a new directory to keep this build isolated.' >&2
    exit 2
fi
mkdir -p -- "$build_dir"
cd -- "$build_dir"
repo init -u https://android.googlesource.com/kernel/manifest \
    -b 75faf58baa4a84e46094af70ab882590fad58fa7 --depth=1
mkdir -p .repo/local_manifests
python3 - "$project_dir/manifests/android-7.3-pinned.xml" <<'PY'
import sys
import xml.etree.ElementTree as ET
root = ET.Element('manifest')
for project in ET.parse(sys.argv[1]).getroot().findall('project'):
    ET.SubElement(root, 'extend-project', name=project.attrib['name'],
                  revision=project.attrib['revision'])
ET.indent(root)
ET.ElementTree(root).write('.repo/local_manifests/avr-pins.xml', encoding='utf-8', xml_declaration=True)
PY
repo sync -c -j"${SYNC_JOBS:-4}"
mkdir -p artifacts
repo manifest -r -o "$build_dir/artifacts/resolved-manifest.xml"
tools/bazel run //common:kernel_aarch64_dist -- --dist_dir="$build_dir/artifacts/gki"
echo "Baseline artifacts: $build_dir/artifacts/gki"
echo 'These artifacts do not include avr vendor drivers.'
