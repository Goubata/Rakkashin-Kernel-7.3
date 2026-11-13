#!/usr/bin/env python3
"""Find firmware-matched official source refs without GitHub API rate limits."""
import concurrent.futures
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BRANCH = 'android-16-release-w3wwl36.65-11-18'

def probe(name):
    url = f'https://github.com/MotorolaMobilityLLC/{name}.git'
    try:
        r = subprocess.run(['git','ls-remote','--exit-code',url,'refs/heads/'+BRANCH],
                           capture_output=True,text=True,timeout=45)
        if r.returncode == 0:
            return {'repository':name,'url':url,'branch':BRANCH,'commit':r.stdout.split()[0]}
        return {'repository':name,'status':'no matching ref' if r.returncode==2 else 'lookup failed',
                'detail':r.stderr.strip()[:300]}
    except subprocess.TimeoutExpired:
        return {'repository':name,'status':'lookup timeout'}

def main():
    repos = json.loads((ROOT.parent/'avr-upstream-audit/repos.json').read_text())
    names = [r['name'] for r in repos if r['name'].startswith(('kernel','vendor-qcom'))
             or r['name']=='motorola-kernel-modules']
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(probe,names))
    (ROOT/'reports/vendor-source-refs.json').write_text(json.dumps(results,indent=2)+'\n')
    print(f'{sum("commit" in r for r in results)} matching source refs out of {len(results)} repositories checked')

if __name__=='__main__':main()
