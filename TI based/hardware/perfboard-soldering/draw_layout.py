from pathlib import Path
from html import escape
import json
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
COLORS = {'supply':'#cf342e','ground':'#334155','dac':'#bc7511','bias':'#078357','out':'#c02f75','sense':'#6847ae'}
# One insulated jumper per entry. Coordinates always refer to the component side.
WIRES = [
 ('01','supply','G16','J14','ADS VIN → C2 supply',['G16','I16','I14','J14'],(15,8.6)),
 ('02','supply','J14','K14','C2 supply → U1 pin 8',['J14','K14'],(14.7,9.6)),
 ('03','ground','G9','N11','DAC GND → U1 pin 4',['G9','H9','N9','N11'],(8.5,10)),
 ('04','ground','N11','J12','U1 pin 4 → C2 ground',['N11','N10','J10','J12'],(10.2,8.7)),
 ('05','ground','G20','G17','ADS ADDR → ADS GND',['G20','H20','H17','G17'],(19,7.5)),
 ('06','ground','N11','Q24','U1 pin 4 → sensor return',['N11','Q11','Q24'],(20,16.5)),
 ('07','dac','G8','M11','DAC VA → U1 pin 3',['G8','H8','M8','M11'],(7.4,10.8)),
 ('08','bias','K11','L11','U1 pin 1 → pin 2',['K11','L11'],(10.2,10.5)),
 ('09','bias','K11','N14','U1 pin 1 → pin 5',['K11','K12','O12','O14','N14'],(12.5,13.5)),
 ('10','bias','K11','B19','U1 pin 1 → ADS A1',['K11','I11','I13','A13','A19','B19'],(15.2,0.5)),
 ('11','out','L14','B20','U1 pin 7 → ADS A0',['L14','L23','A23','A20','B20'],(23.5,4)),
 ('12','out','L14','M17','U1 pin 7 → C1 output',['L14','L17','M17'],(16,10.6)),
 ('13','out','M17','O22','C1 output → R1 output',['M17','N17','N22','O22'],(20,12.5)),
 ('14','sense','M14','M16','U1 pin 6 → C1 sense',['M14','M16'],(15,11.5)),
 ('15','sense','M14','O15','U1 pin 6 → R1 sense',['M14','O15'],(14.1,13.5)),
 ('16','sense','M14','P24','U1 pin 6 → sensor sense',['M14','P14','P24'],(20,15.5)),
]

# Verify intended net connectivity, including the common ground in the QT cable.
parent={}
def root(a):
 parent.setdefault(a,a)
 if parent[a]!=a: parent[a]=root(parent[a])
 return parent[a]
def join(a,b):parent[root(a)]=root(b)
for _,_,a,b,_,route,_ in WIRES:
 assert route[0]==a and route[-1]==b
 join(a,b)
join('G9','G17') # QT cable ground; not another perfboard jumper.
groups={
 'supply':['G16','J14','K14'],
 'ground':['G9','G17','G20','N11','J12','Q24'],
 'dac':['G8','M11'],
 'bias':['K11','L11','N14','B19'],
 'out':['L14','B20','M17','O22'],
 'sense':['M14','M16','O15','P24'],
}
for name,coords in groups.items():assert len({root(a) for a in coords})==1, name
assert len({root(v[0]) for v in groups.values()})==6, 'Unintended net short'

W,H=1860,1220
im=Image.new('RGB',(W,H),'#f8fafc');d=ImageDraw.Draw(im);fonts={}
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">',f'<rect width="{W}" height="{H}" fill="#f8fafc"/>']
def text(x,y,t,size=17,color='#243447',bold=False,anchor='start'):
 key=(size,bold)
 if key not in fonts:fonts[key]=ImageFont.truetype('C:/Windows/Fonts/'+('segoeuib.ttf' if bold else 'segoeui.ttf'),size)
 d.text((x,y),t,font=fonts[key],fill=color,anchor={'start':'ls','middle':'ms','end':'rs'}[anchor])
 svg.append(f'<text x="{x}" y="{y}" font-family="Segoe UI,Arial,sans-serif" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{700 if bold else 400}">{escape(t)}</text>')
def line(points,color,width=3):
 d.line(points,fill=color,width=width)
 svg.append('<polyline points="'+' '.join(f'{x},{y}' for x,y in points)+f'" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"/>')
def rect(x,y,w,h,fill,stroke='#64748b',radius=4):
 d.rounded_rectangle((x,y,x+w,y+h),radius=radius,fill=fill,outline=stroke,width=2)
 svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}" stroke-width="2"/>')
def circle(x,y,r,fill,stroke='#927840'):
 d.ellipse((x-r,y-r,x+r,y+r),fill=fill,outline=stroke,width=1)
 svg.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="{stroke}"/>')
def grid(c,r,side=0):return (40+side*920+90+((24-c) if side else (c-1))*28,185+r*28)
def pt(a,side=0):return grid(int(a[1:]),ord(a[0])-65,side)
def header(a,b,side=0):
 x,y=pt(a,side);xx,yy=pt(b,side)
 rect(min(x,xx)-9,min(y,yy)-9,abs(xx-x)+18,abs(yy-y)+18,'#334155')
 n=max(abs(int(a[1:])-int(b[1:])),abs(ord(a[0])-ord(b[0])))
 for i in range(n+1):circle(x+(xx-x)*i/max(n,1),y+(yy-y)*i/max(n,1),4,'#162130','#e9c76f')

text(40,44,'Perfboard soldering plan — ADS1115 rotated 180°',30,bold=True)
text(40,77,'MCP6002 ground from MCP4728 • MCP6002 supply from ADS1115 VIN • insulated underside jumpers',19)
for side,title in [(0,'1. COMPONENT SIDE — install sockets and parts'),(1,'2. SOLDER SIDE — board flipped left-to-right')]:
 off=40+920*side
 text(off,120,title,22,bold=True)
 rect(off+26,147,772,552,'#e5e9dc')
 for c in range(1,25):
  x,y=grid(c,0,side);text(x,171,str(c),13,anchor='middle')
 for rr in range(18):
  y=185+rr*28;text(off+10,y+5,chr(65+rr),14,anchor='middle')
  for c in range(1,25):circle(*grid(c,rr,side),5,'#fff' if not side else '#debd76')
 for xx in [off+48,off+776]:
  for yy in [169,677]:circle(xx,yy,10,'#f8fafc')
 # Corner registration marks prevent a top/bottom flip mistake.
 x,y=pt('A1',side);circle(x,y,7,'#d83335');text(x+(14 if not side else -14),y-10,'A1',12,'#be282d',True,'start' if not side else 'end')

# Component side only: modules, readable pin labels, DIP socket.
for c,name,top,bottom,col in [
 (4,'MCP4728',['RDY','LDAC','SDA','SCL','GND','VCC'],['VCC','VD','VC','VB','VA','GND'],'#eee3f5'),
 (16,'ADS1115 · 180°',['A+','A3','A2','A1','A0','A−'],['VIN','GND','SCL','SDA','ADDR','ALRT'],'#e0f0f3')]:
 x,y=pt('B'+str(c));rect(x-70,y-28,280,196,col)
 text(x+70,y+61,name,23,bold=True,anchor='middle')
 text(x+70,y+86,'QT sockets at left / right edges',13,anchor='middle')
 for row,labels in [('B',top),('G',bottom)]:
  header(row+str(c),row+str(c+5))
  for i,label in enumerate(labels):
   xx,yy=pt(row+str(c+i));text(xx,yy+(-14 if row=='B' else 25),label,11,anchor='middle')

x,y=pt('K11');rect(x-12,y-18,108,122,'#334155')
line([(x+30,y-18),(x+30,y-7),(x+54,y-7),(x+54,y-18)],'#f8fafc',2)
text(x+42,y+39,'MCP',16,'white',True,'middle');text(x+42,y+60,'6002',16,'white',True,'middle')
for i in range(4):
 for c,p in [(11,i+1),(14,8-i)]:
  xx,yy=grid(c,10+i);circle(xx,yy,5,'#152030','#e5c97b');text(xx+(-17 if c==11 else 17),yy+4,str(p),13,anchor='middle')
header('O15','O22');x,y=pt('O15');xx,_=pt('O22')
line([(x,y),(x,y+30),(xx,y+30),(xx,y)],'#986637',2);rect(x+22,y+15,152,36,'#f1e0c6','#986637');text(x+98,y+39,'R1 · 1 MΩ',16,anchor='middle')
for a,b,label in [('M16','M17','C1'),('J12','J14','C2')]:
 x,y=pt(a);xx,yy=pt(b);line([(x,y),(xx,yy)],'#977820',2);rect((x+xx)/2-10,y-8,20,16,'#ebc544');text((x+xx)/2,y-16,label,14,anchor='middle')
header('P24','Q24')
text(830,611,'SENSE',12,COLORS['sense']);text(830,638,'GND',12)
line([pt('P24'),(825,605)],COLORS['sense']);line([pt('Q24'),(825,633)],COLORS['ground'])
text(80,734,'External QT cable → LEFT edge. Sensor clamp pair → RIGHT edge.',17,bold=True)
text(80,761,'Module-to-module QT cable stays above the boards; plug clearance must be checked.',15)
text(80,786,'C1: M16–M17, 100 nF.  C2: J12–J14, 100 nF.  R1: O15–O22.',16)

# Underside: socket pads outlined, then insulated wires. White edging shows overpasses.
occupied=[]
for row,cs in [('B',range(4,10)),('G',range(4,10)),('B',range(16,22)),('G',range(16,22))]:
 occupied.extend(row+str(c) for c in cs)
occupied += [r+str(c) for r in 'KLMN' for c in [11,14]]
occupied += ['O'+str(c) for c in range(15,23)]+['J12','J14','M16','M17','P24','Q24']
for a in occupied:circle(*pt(a,1),7,'#f7d484','#334155')
for ident,net,a,b,desc,route,tag in WIRES:
 coords=[pt(p,1) for p in route]
 line(coords,'#f8fafc',8);line(coords,COLORS[net],4)
for ident,net,a,b,desc,route,tag in WIRES:
 for p in [a,b]:circle(*pt(p,1),4,COLORS[net],COLORS[net])
 x,y=grid(*tag,side=1);rect(x-13,y-10,26,20,'#fff',COLORS[net],4);text(x,y+5,ident,12,COLORS[net],True,'middle')
text(1000,734,'Solder only the two listed endpoints of each jumper.',17,bold=True)
text(1000,761,'Crossings are insulated overpasses. Passing a hole does NOT connect it.',16)
text(1000,786,'Columns run 24 → 1 here; rows stay A → R. Red A1 marks the same physical hole.',15)

text(40,830,'JUMPER LIST — coordinates identify the same physical holes in both views',22,bold=True)
for i,(ident,net,a,b,desc,_,__) in enumerate(WIRES):
 x=40+(i//8)*920;y=865+(i%8)*30
 rect(x,y-18,32,23,'#fff',COLORS[net]);text(x+16,y,ident,14,COLORS[net],True,'middle')
 text(x+47,y,f'{a} ↔ {b}',18,COLORS[net],True)
 text(x+200,y,desc,18)
text(40,1135,'Use this as a new-build plan, not a tracing of your existing solder joints. Socket endpoints for R1 are assumed to be positions 1 and 8.',17,bold=True)
text(40,1165,'Dry-fit modules, resistor and QT plugs first. C1 = 100 nF follows V1; keep your verified value if it differs. QT power must be 3.3 V.',17)
text(40,1195,'Before inserting modules: inspect for bridges and verify jumper continuity / no supply-to-ground short. This image is not a 1:1 drilling template.',16)
svg.append('</svg>')
(OUT/'soldering-plan.svg').write_text('\n'.join(svg),encoding='utf-8')
im.save(OUT/'soldering-plan.png')
(OUT/'jumpers.json').write_text(json.dumps([dict(id=w[0],net=w[1],start=w[2],end=w[3],connection=w[4]) for w in WIRES],indent=2),encoding='utf-8')
md=['# Perfboard soldering plan','',
'New-build plan: ADS1115 rotated 180° relative to the initial layout. MCP6002 pin 4 goes to MCP4728 GND; pin 8 is fed from ADS1115 VIN via C2. Supply is 3.3 V from the QT chain.',
'','## Assembly','',
'1. Mark A1 on the component side. Match the illustrated numbering before soldering; physical board labels may face a different direction.',
'2. Dry-fit the sockets and modules. DAC headers: B4–B9 / G4–G9; ADC: B16–B21 / G16–G21. Check QT plug access.',
'3. Install the DIP-8 socket at K11–N11 / K14–N14, notch toward A. Pin 1 = K11; pin 4 = N11; pin 8 = K14; pin 5 = N14.',
'4. Install the resistor socket at O15–O22 (end contacts assumed), C1 at M16–M17, C2 at J12–J14, and sensor contacts P24/Q24. Check actual resistor body and lead spacing.',
'5. Flip the board left-to-right while keeping edge A at the top. The solder view reverses columns, not rows.',
'6. Fit insulated jumpers in numerical order. Solder only endpoints; leave insulation intact where wires cross or pass other pads. Crossing wires never join unless the table shares an endpoint.',
'7. Inspect and continuity-check the named nets before fitting IC/modules. Verify no hard short from supply to ground and verify 3.3 V at the empty socket pin 8 relative to pin 4. Disconnect power before inserting modules.',
'','## Jumpers','','| ID | From | To | Connection |','|---|---|---|---|']
md += [f'| {w[0]} | {w[2]} | {w[3]} | {w[4]} |' for w in WIRES]
md += ['','## Parts and external connections','',
'- C1: 100 nF per V1, between pin-6 sense and pin-7 output. C2: 100 nF between supply and ground, near the DIP socket.',
'- R1: 1 MΩ between O15 (sense) and O22 (output). The sensor is between P24 (sense) and Q24 (ground), not parallel with R1.',
'- CC2650 interface cable exits left into MCP4728 QT. The other QT socket links to ADS1115. Confirm the CC2650 interface wiring; no controller pinout is assumed.',
'- QT provides the common ground between modules. W05 grounds ADDR locally at ADS1115, selecting 0x48.',
'- Drawings assume the previously specified 24 × 18 grid at 2.54 mm pitch. Board offsets, socket bodies and cable clearance are not fabrication-verified. Do not print this as a drilling template.',
'','## Validation','',
'The generator checks that all 16 jumpers connect the intended six separate electrical nets, including QT ground continuity. It does not electrically test the physical assembly.']
(OUT/'assembly.md').write_text('\n'.join(md)+'\n',encoding='utf-8')
print('Generated SVG, PNG, jumper list and assembly guide. Connectivity check passed: 16 wires, 6 separate nets.')
