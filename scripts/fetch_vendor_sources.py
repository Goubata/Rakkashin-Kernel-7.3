#!/usr/bin/env python3
"""Fetch pinned official source metadata, leaving checkout to each port stage."""
import concurrent.futures
import json
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT.parent/'avr-upstream-audit/vendor-repos'

def run(args):
    return subprocess.check_output(args,stderr=subprocess.STDOUT,text=True,timeout=180)

def fetch(item):
    name=item['repository']; dest=DEST/name
    if name=='kernel-msm':
        return {'repository':name,'path':'../avr-upstream-audit/qcom-soc','status':'existing SoC reference'}
    try:
        if not (dest/'.git').exists():
            dest.mkdir(parents=True,exist_ok=True)
            run(['git','init','-q',str(dest)])
            run(['git','-C',str(dest),'remote','add','origin',item['url']])
            run(['git','-C',str(dest),'config','remote.origin.promisor','true'])
            run(['git','-C',str(dest),'config','remote.origin.partialclonefilter','blob:none'])
        try:head=run(['git','-C',str(dest),'rev-parse','--verify','HEAD']).strip()
        except subprocess.CalledProcessError:head=None
        if head and head != item['commit']:
            return {'repository':name,'status':'existing checkout differs; left untouched'}
        if not head:
            run(['git','-C',str(dest),'fetch','--filter=blob:none','--depth=1','origin',item['commit']])
            run(['git','-C',str(dest),'update-ref','refs/heads/firmware',item['commit']])
            run(['git','-C',str(dest),'symbolic-ref','HEAD','refs/heads/firmware'])
        return {'repository':name,'commit':item['commit'],'path':str(dest),'status':'metadata fetched; no working checkout yet'}
    except (subprocess.CalledProcessError,subprocess.TimeoutExpired) as e:
        return {'repository':name,'status':'fetch failed','detail':str(e)[:400]}

def main():
    items=[i for i in json.loads((ROOT/'reports/vendor-source-refs.json').read_text()) if 'commit' in i]
    DEST.mkdir(exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        result=list(pool.map(fetch,items))
    (ROOT/'reports/vendor-source-fetch.json').write_text(json.dumps(result,indent=2)+'\n')
    print('Fetched metadata:',sum(i['status'].startswith('metadata') for i in result),'failures:',sum(i['status']=='fetch failed' for i in result))

if __name__=='__main__':main()
