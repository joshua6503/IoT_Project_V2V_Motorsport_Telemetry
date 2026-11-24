import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
fig, ax = plt.subplots(figsize=(16, 9)); ax.set_xlim(0,16); ax.set_ylim(0,9); ax.axis('off')
def box(x,y,w,h,title,body,fc='white'):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle='round,pad=0.08',fc=fc,ec='black',lw=1.5))
    ax.text(x+w/2,y+h-0.3,title,ha='center',va='top',fontsize=13,weight='bold')
    ax.text(x+w/2,y+h-0.8,body,ha='center',va='top',fontsize=10,linespacing=1.4)
def arrow(x1,y1,x2,y2,txt='',style='-|>',ls='-',off=(0,0.18),both=False):
    ax.annotate('',xy=(x2,y2),xytext=(x1,y1),arrowprops=dict(arrowstyle='<|-|>' if both else style,lw=1.6,ls=ls,color='black'))
    if txt: ax.text((x1+x2)/2+off[0],(y1+y2)/2+off[1],txt,ha='center',va='bottom',fontsize=9.5)
ax.text(8,8.7,'V2V Motorsport Telemetry — arquitectura MP2',ha='center',fontsize=16,weight='bold')
box(0.3,5.2,4.2,2.9,'Nodo A · FRDM-RW612','rw612-a · FreeRTOS + lwIP\nHC-SR04 · DHT11 · pot freno (FIR)\nalert_evaluate() local\nLEDs + buzzer activo')
box(0.3,0.9,4.2,2.9,'Nodo B · ESP32 DevKit','esp32-b · Arduino + esp-mqtt\npot freno · botón BOOT (manual)\nalert_evaluate() local\nLEDs + buzzer pasivo (PWM)')
box(6.9,3.0,3.4,3.1,'Red Wi-Fi 2.4 GHz','Desarrollo: Wi-Fi de casa\n(o hotspot)\n\nDemo: AP "V2V-PIT"\ncreado por la Pi\n(WPA2, sin internet)')
box(11.4,4.9,4.3,3.2,'Raspberry Pi 5 · gateway','Mosquitto (MQTT, puerto 1883)\nusuario/contraseña por nodo\nmodo casa ↔ modo pits\n(pi-mode con rollback)\nBroker en pits: 10.42.0.1')
box(11.4,0.9,4.3,2.9,'Laptop · dashboard','Node-RED + Dashboard 2.0\nestado en línea / fuera de línea\nmedidores, gráficas 5 min\ntabla de alertas, banner crítico')
arrow(4.5,5.6,6.9,5.6,'MQTT: telemetría 5 Hz,\nalertas, status/LWT',off=(0,0.1))
arrow(4.5,3.4,6.9,3.4,'MQTT: telemetría 5 Hz,\nalertas, status/LWT',off=(0,0.1))
arrow(10.3,5.4,11.4,5.4,'',both=True)
arrow(13.55,4.9,13.55,3.8,'suscripción v2v/#',off=(1.15,-0.15))
arrow(2.4,5.2,2.4,3.8,'BLE V2V\n(fase siguiente, F5)',ls='--',both=True,off=(1.25,-0.25))
ax.text(8,0.25,'Camino de seguridad: cada nodo decide y actúa localmente (sin red). MQTT es informativo: telemetría, alertas espejo y estado para el dashboard.',ha='center',fontsize=10,style='italic')
fig.savefig('sistema_mp2.png',dpi=150,bbox_inches='tight',facecolor='white'); fig.savefig('sistema_mp2.svg',bbox_inches='tight',facecolor='white')
