"""Read-only PE exports/imports and focused disassembly for the engine ABI."""
import argparse,json,hashlib
from pathlib import Path
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_32

p=argparse.ArgumentParser();p.add_argument('path');p.add_argument('--filter',default='');p.add_argument('--asm');p.add_argument('--size',type=lambda s:int(s,0),default=512);p.add_argument('--out');a=p.parse_args()
data=Path(a.path).read_bytes();pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
exports={e.name.decode():base+e.address for e in getattr(pe,'DIRECTORY_ENTRY_EXPORT',[]).symbols if e.name} if hasattr(pe,'DIRECTORY_ENTRY_EXPORT') else {}
imports={base+i.address-base:(d.dll.decode(),i.name.decode() if i.name else str(i.ordinal)) for d in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[]) for i in d.imports}
report=dict(path=str(Path(a.path).resolve()),sha256=hashlib.sha256(data).hexdigest(),image_base=hex(base),exports={k:hex(v) for k,v in exports.items()},imports={hex(k):v for k,v in imports.items()})
if a.out:Path(a.out).write_text(json.dumps(report,indent=2)+'\n')
if a.asm:
 address=exports.get(a.asm) or int(a.asm,0)
 code=pe.get_data(address-base,a.size)
 reverse={v:k for k,v in exports.items()}
 for i in Cs(CS_ARCH_X86,CS_MODE_32).disasm(code,address):
  note=''
  try:
   target=int(i.op_str,16);note=reverse.get(target,'')
  except ValueError:pass
  for k,v in imports.items():
   if hex(k) in i.op_str:note='!'.join(v)
  print(hex(i.address),i.mnemonic,i.op_str,note)
else:
 for name,address in exports.items():
  if a.filter.lower() in name.lower():print(hex(address),name)
