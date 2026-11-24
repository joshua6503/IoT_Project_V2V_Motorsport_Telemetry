import schemdraw, schemdraw.elements as elm
schemdraw.config(fontsize=11, lw=1.3)
d = schemdraw.Drawing(show=False)
pins=[('3V3','3V3 (J3)'),('5V','5V (J3-10)'),('D3','D3 · GPIO15 (J1-8)'),('D4','D4 · GPIO18 (J1-10)'),
('D5','D5 · GPIO27 (J1-12)'),('D6','D6 · GPIO0 (J1-14)'),('D7','D7 · GPIO20 (J1-16)'),('D8','D8 · GPIO50 (J2-2)'),
('D9','D9 · GPIO52 (J2-4)'),('J73','J7-3 · GPIO46 (ADC CH4)'),('A0','A0 · GPIO42 (ADC, canal TBD)'),('GND','GND')]
n=len(pins)
ic = elm.Ic(pins=[elm.IcPin(name=lbl, anchorname=k, side='right', slot=f'{n-i}/{n}') for i,(k,lbl) in enumerate(pins)],
            size=(10, 15), pinspacing=1.2, edgepadH=0.6)
d += ic
d.add(elm.Label().at((ic.GND[0]-9.6, (ic['3V3'][1]+ic.GND[1])/2)).label('FRDM-RW612\n\nrw612-a\n\nUSB MCU-Link\n(J10) → laptop', halign='left', fontsize=12))
def net(k, text):
    d.add(elm.Line().at(getattr(ic,k)).right(1.2)); d.add(elm.Dot(open=True)); d.add(elm.Label().label(text, loc='right', ofst=0.1))
for k,t in [('3V3','3V3'),('5V','5V'),('D6','BUZZ'),('D7','TRIG'),('D8','ECHO'),('D9','DHT'),('J73','FRENO'),('A0','TPS')]: net(k,t)
d.add(elm.Line().at(ic.GND).right(1.2)); d.add(elm.Ground())
for k,col in [('D3','verde'),('D4','amarillo'),('D5','rojo')]:
    d.add(elm.Line().at(getattr(ic,k)).right(1.2)); d.add(elm.Resistor().right().label('330 Ω'))
    d.add(elm.LED().right().label(f'LED {col}', loc='bottom')); d.add(elm.Line().right(0.4)); d.add(elm.Ground())
X0 = ic['3V3'][0]; top = ic['3V3'][1] + 0.8
# Buzzer (active)
bx = X0 + 15
d.add(elm.Dot(open=True).at((bx, top))); d.add(elm.Label().at((bx, top)).label('5V', loc='top'))
d.add(elm.Line().at((bx, top)).down(0.6)); A = d.here
d.add(elm.RBox().down().length(2.2)); B = d.here
d.add(elm.Line().down(0.6))
q = d.add(elm.BjtNpn(circle=True).theta(0).anchor('collector'))
d.add(elm.Line().at(A).left(1.4)); d.add(elm.Dot().at(A)); d.add(elm.Line().at(B).left(1.4)); d.add(elm.Dot().at(B))
d.add(elm.Diode().at((A[0]-1.4, B[1])).to((A[0]-1.4, A[1])))
d.add(elm.Label().at((A[0]-1.7, (A[1]+B[1])/2)).label('Diodo 1N4148\n(banda a 5V)', halign='right', fontsize=9))
d.add(elm.Label().at((bx+1.4, top-1.9)).label('Buzzer activo\nTMB12A05\n+ arriba (5V)', halign='left'))
d.add(elm.Label().at((bx+1.4, top-5.0)).label('NPN 2N2222A', halign='left'))
d.add(elm.Line().at(q.emitter).down(0.5)); d.add(elm.Ground())
d.add(elm.Resistor().at(q.base).left().label('1 kΩ')); d.add(elm.Dot(open=True)); d.add(elm.Label().label('BUZZ', loc='left', ofst=0.1))
# HC-SR04
hy = top - 9.5
hc = elm.Ic(pins=[elm.IcPin(name='VCC', anchorname='vcc', side='left', slot='4/4'),elm.IcPin(name='TRIG', anchorname='trig', side='left', slot='3/4'),
                  elm.IcPin(name='ECHO', anchorname='echo', side='left', slot='2/4'),elm.IcPin(name='GND', anchorname='gnd', side='left', slot='1/4')],
            size=(3.2,4), pinspacing=1.0).theta(0).at((bx+2.5, hy)).anchor('vcc')
d += hc
d.add(elm.Label().at((bx+4.1, hy+0.9)).label('HC-SR04 (5 V)', fontsize=12))
d.add(elm.Line().at(hc.vcc).left(1)); d.add(elm.Dot(open=True)); d.add(elm.Label().label('5V', loc='left'))
d.add(elm.Line().at(hc.trig).left(1)); d.add(elm.Dot(open=True)); d.add(elm.Label().label('TRIG', loc='left'))
d.add(elm.Line().at(hc.gnd).left(0.5)); d.add(elm.Ground())
d.add(elm.Line().at(hc.echo).left(1.0)); E = d.here
d.add(elm.Resistor().left().label('2 kΩ')); d.add(elm.Dot()); M = d.here
d.add(elm.Line().left(0.8)); d.add(elm.Dot(open=True)); d.add(elm.Label().label('ECHO (≈2.9 V)', loc='left'))
d.add(elm.Resistor().at(M).down().label('2.7 kΩ', loc='bottom')); d.add(elm.Ground())
# DHT11
dy = hy - 6.5
dh = elm.Ic(pins=[elm.IcPin(name='VCC', anchorname='vcc', side='left', slot='3/3'),elm.IcPin(name='DATA', anchorname='data', side='left', slot='2/3'),
                  elm.IcPin(name='GND', anchorname='gnd', side='left', slot='1/3')], size=(3.2,3), pinspacing=1.0).theta(0).at((bx+2.5, dy)).anchor('vcc')
d += dh
d.add(elm.Label().at((bx+4.1, dy+0.9)).label('DHT11 (módulo, 3.3 V)', fontsize=12))
for a,t in [('vcc','3V3'),('data','DHT')]:
    d.add(elm.Line().at(getattr(dh,a)).left(1)); d.add(elm.Dot(open=True)); d.add(elm.Label().label(t, loc='left'))
d.add(elm.Line().at(dh.gnd).left(0.5)); d.add(elm.Ground())
d.add(elm.Label().at((bx+8.2, dy-1)).label('Sensor sin módulo:\npull-up 5.1 kΩ\nDATA → 3V3', halign='left', fontsize=9))
# Pots
for i,(nm,rt,rp,txt) in enumerate([('FRENO','5.1 kΩ','Pot 2 kΩ\n(freno)','0.10–1.00 V'),('TPS','2.7 kΩ','Pot 1 kΩ\n(TPS, opcional)','0.19–1.03 V')]):
    px = bx + 12 + i*10
    d.add(elm.Dot(open=True).at((px, top))); d.add(elm.Label().at((px,top)).label('3V3', loc='top'))
    d.add(elm.Resistor().at((px, top)).down().label(rt, loc='bottom'))
    p = d.add(elm.Potentiometer().down())
    d.add(elm.Label().at((px-1.0, (p.start[1]+p.end[1])/2)).label(rp, halign='right'))
    d.add(elm.Resistor().down().label('220 Ω', loc='bottom')); d.add(elm.Ground())
    d.add(elm.Line().at(p.tap).right(1.0)); d.add(elm.Dot(open=True))
    d.add(elm.Label().label(f'{nm}\n{txt}', loc='right', ofst=0.1))
d.add(elm.Label().at((ic.GND[0]-9.6, ic.GND[1]-2.6)).label(
 'Mismo nombre = mismo punto (3V3, 5V, BUZZ, TRIG, ECHO, DHT, FRENO, TPS). Todas las tierras al GND de la tarjeta.\n'
 'Valores de hardware/pinout.md (DEC-005, DEC-011); voltajes calculados, no medidos. El ADC de la RW612 usa referencia de 1.2 V,\n'
 'por eso los divisores de los potenciómetros. ECHO del HC-SR04 bajado de 5 V a ≈2.9 V (tolerancia a 5 V: OPEN-015).', loc='right', halign='left'))
d.save('rw612_conexiones.svg'); d.save('rw612_conexiones.png', dpi=150)
