# Browser addon packaging for Dragon Download Manager. LGPL v3.
# Derived packaging layout: Copyright 2019-present Sebastien Vavassori.
import argparse,json,shutil,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
def make_addons(current_path=None, output_path=None, *, archives=None):
    output = output_path or ROOT / "dist/browser-extension"
    archives = archives or ROOT / "dist"
    version=(ROOT/'version').read_text(encoding='utf-8').strip()
    source=Path(current_path)/'src' if current_path else ROOT/'web-extension/extension/src'
    output=Path(output).resolve();archives=Path(archives).resolve()
    output.mkdir(parents=True,exist_ok=True);archives.mkdir(parents=True,exist_ok=True)
    result = {}
    for engine in ['chromium','firefox']:
        target=output/engine
        shutil.copytree(source/'base',target,dirs_exist_ok=True)
        shutil.copytree(source/engine,target,dirs_exist_ok=True)
        manifest=target/'manifest.json'
        data=json.loads(manifest.read_text(encoding='utf-8'));data['version']=version
        manifest.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
        archive=archives/f'DragonDownloadManager_{engine}_v{version}.zip'
        files={str(p.relative_to(source/'base')) for p in (source/'base').rglob('*') if p.is_file()}
        files.update(str(p.relative_to(source/engine)) for p in (source/engine).rglob('*') if p.is_file())
        with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
            for name in sorted(files):z.write(target/name,name)
        print(archive)
        result[engine] = str(archive)
    return result
if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('-o','--output-directory',default=str(ROOT/'dist/browser-extension'))
    parser.add_argument('--archive-directory',default=str(ROOT/'dist'))
    args=parser.parse_args();make_addons(output_path=args.output_directory,archives=args.archive_directory)
