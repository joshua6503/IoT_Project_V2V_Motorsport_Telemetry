import schemdraw, schemdraw.elements as elm
schemdraw.config(fontsize=11, lw=1.3)
d = schemdraw.Drawing(show=False)
ic = elm.Ic(pins=[
    elm.IcPin(name='3V3', side='right', slot='8/8'),
    elm.IcPin(name='GPIO32 (ADC1_CH4)', side='right', slot='7/8'),
    elm.IcPin(name='GPIO25', side='right', slot='6/8'),
    elm.IcPin(name='GPIO26', side='right', slot='5/8'),
    elm.IcPin(name='GPIO27', side='right', slot='4/8'),
    elm.IcPin(name='GPIO33', side='right', slot='3/8'),
    elm.IcPin(name='VIN (5 V)', side='right', slot='2/8'),
    elm.IcPin(name='GND', side='right', slot='1/8'),
], size=(6.5, 11), pinspacing=1.3, edgepadH=0.6, label='', lblsize=12)
d += ic
d.add(elm.Label().at((ic['GND'][0]-6.2, (ic['3V3'][1]+ic['GND'][1])/2)).label('ESP32\nDevKit\n\nesp32-b\n\nUSB → laptop\n(COM3)\n\nBOOT =\nGPIO0\n(en la placa)', halign='left', fontsize=11))
def net(pin, text):
    d.add(elm.Line().at(getattr(ic, pin)).right(1.2)); d.add(elm.Dot(open=True))
    d.add(elm.Label().label(text, loc='right', ofst=0.1))
net('3V3','3V3'); net('GPIO32 (ADC1_CH4)','FRENO'); net('GPIO33','BUZZ'); net('VIN (5 V)','5V')
d.add(elm.Line().at(ic.GND).right(1.2)); d.add(elm.Ground())
for pin, col in [('GPIO25','verde'),('GPIO26','amarillo'),('GPIO27','rojo')]:
    d.add(elm.Line().at(getattr(ic, pin)).right(1.2))
    d.add(elm.Resistor().right().label('220 Ω'))
    d.add(elm.LED().right().label(f'LED {col}', loc='bottom'))
    d.add(elm.Line().right(0.4)); d.add(elm.Ground())
# Buzzer column
bx = ic['3V3'][0] + 14; top = ic['3V3'][1] + 0.8
d.add(elm.Dot(open=True).at((bx, top))); d.add(elm.Label().at((bx, top)).label('5V', loc='top'))
d.add(elm.Line().at((bx, top)).down(0.6))
A = d.here
sp = d.add(elm.RBox().down().length(2.2))
B = d.here
d.add(elm.Line().down(0.6))
q = d.add(elm.BjtNpn(circle=True).theta(0).anchor('collector'))
# flyback diode in parallel with buzzer (cathode to 5V)
d.add(elm.Line().at(A).left(1.4)); d.add(elm.Dot().at(A))
d.add(elm.Line().at(B).left(1.4)); d.add(elm.Dot().at(B))
d.add(elm.Diode().at((A[0]-1.4, B[1])).to((A[0]-1.4, A[1])))
d.add(elm.Label().at((A[0]-1.7, (A[1]+B[1])/2)).label('Diodo 1N4148\n(banda a 5V)\nopcional', halign='right', fontsize=9))
d.add(elm.Label().at((bx+1.4, top-1.9)).label('Buzzer pasivo\n(PWM 2.5 kHz)\n+ arriba (5V)\n− al colector', halign='left'))
d.add(elm.Label().at((bx+1.4, top-5.0)).label('NPN 2N2222A\ncara plana al frente:\nE · B · C', halign='left'))
d.add(elm.Line().at(q.emitter).down(0.5)); d.add(elm.Ground())
d.add(elm.Resistor().at(q.base).left().label('1 kΩ'))
d.add(elm.Dot(open=True)); d.add(elm.Label().label('BUZZ', loc='left', ofst=0.1))
# Pot column
px = bx + 10
d.add(elm.Dot(open=True).at((px, top))); d.add(elm.Label().label('3V3', loc='top'))
d.add(elm.Resistor().down().label('5.6 kΩ', loc='bottom'))
pot = d.add(elm.Potentiometer().down())
d.add(elm.Label().at((px-1.0, (pot.start[1]+pot.end[1])/2)).label('Pot 50 kΩ\n(freno)', halign='right'))
d.add(elm.Resistor().down().label('2.7 kΩ', loc='bottom'))
d.add(elm.Ground())
d.add(elm.Line().at(pot.tap).right(1.0)); d.add(elm.Dot(open=True))
d.add(elm.Label().label('FRENO', loc='right', ofst=0.1))
d.add(elm.Label().at((ic['GND'][0]-4.5, ic['GND'][1]-2.4)).label(
 'Mismo nombre = mismo punto (3V3, 5V, FRENO, BUZZ). Todas las tierras van al GND del ESP32.\n'
 'LED: pata larga (ánodo) hacia la resistencia, pata corta a GND.   Rango del cursor ≈ 0.15–2.98 V (calculado, no medido).\n'
 'Diodo opcional en paralelo con el buzzer (protege al transistor). Buzzer pasivo: suena solo con PWM.', loc='right', halign='left'))
d.save('esp32_conexiones.svg'); d.save('esp32_conexiones.png', dpi=170)
