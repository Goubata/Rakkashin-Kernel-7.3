#!/usr/bin/env python3
"""Adapt legacy clock rounding callbacks to 7.3 rate requests."""
from pathlib import Path
import argparse
import os
import re

ROOT = Path(__file__).resolve().parents[1]
DIRECTORY = ROOT/'ports/qcom-clock-core'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', nargs='?', type=Path, default=DIRECTORY)
    directory = parser.parse_args().directory
    for path in directory.glob('*.c'):
        text = path.read_text()
        pattern = re.compile(r'(?:static\s+)?const\s+struct\s+clk_ops\s+\w+\s*=\s*\{.*?\n\};', re.S)
        seen = set()
        count = 0
        def migrate(match):
            nonlocal count
            block = match.group()
            item = re.search(r'\.round_rate\s*=\s*(\w+)\s*,', block)
            if not item:return block
            assert '.determine_rate' not in block, 'Both rate callbacks require manual review'
            callback = item.group(1)
            adapter = callback + '_request'
            replacement = re.sub(r'\.round_rate\s*=\s*'+callback+r'\s*,', '.determine_rate = '+adapter+',', block)
            count += 1
            if callback in seen:return replacement
            seen.add(callback)
            wrapper = f'''static int {adapter}(struct clk_hw *hw,
\t\t\t\tstruct clk_rate_request *req)
{{
\treturn qcom_vendor_determine_rate(hw, req, {callback});
}}

'''
            return wrapper + replacement
        changed = pattern.sub(migrate, text)
        changed = re.sub(r'\bdivider_round_rate\(', 'qcom_vendor_divider_round_rate(', changed)
        changed = re.sub(r'\bdivider_ro_round_rate\(', 'qcom_vendor_divider_ro_round_rate(', changed)
        raw = r'\tlong rate = (\w+)\(hw, req->rate, &req->best_parent_rate\);\n\n\tif \(rate < 0\)\n\t\treturn rate;\n\treq->rate = rate;\n\treturn 0;'
        changed = re.sub(raw, lambda m: '\treturn qcom_vendor_determine_rate(hw, req, '+m.group(1)+');', changed)
        compat = os.path.relpath(DIRECTORY/'rate-compat.h', path.parent)
        include = '#include "'+compat+'"'
        if any(n in changed for n in ('qcom_vendor_divider_round_rate(', 'qcom_vendor_divider_ro_round_rate(', 'qcom_vendor_determine_rate(')) and include not in changed:
            includes = list(re.finditer(r'^#include[^\n]+$', changed, re.M))
            assert includes
            pos = includes[-1].end()
            changed = changed[:pos] + '\n'+include + changed[pos:]
        if changed != text:
            path.write_text(changed)
            print(path.name, count, 'legacy initializers; bridge normalized')

if __name__ == '__main__':main()
