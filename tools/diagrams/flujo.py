import matplotlib; matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch
fig, ax = plt.subplots(figsize=(16, 8)); ax.set_xlim(0,16); ax.set_ylim(0,8); ax.axis('off')
def box(x,y,w,h,txt,fs=11,bold=False):
    ax.add_patch(FancyBboxPatch((x,y),w,h,boxstyle='round,pad=0.06',fc='white',ec='black',lw=1.4))
    ax.text(x+w/2,y+h/2,txt,ha='center',va='center',fontsize=fs,weight='bold' if bold else 'normal',linespacing=1.35)
def arr(x1,y1,x2,y2): ax.annotate('',xy=(x2,y2),xytext=(x1,y1),arrowprops=dict(arrowstyle='-|>',lw=1.4,color='black'))
ax.text(8,7.7,'Flujo de datos MQTT — topics v2v/{node_id}/…',ha='center',fontsize=16,weight='bold')
box(0.2,3.0,2.4,2.0,'Nodo\nrw612-a\n\nNodo\nesp32-b',12,True)
rows=[('telemetry','QoS 0 · sin retain · 5 Hz','{"node_id","ts_ms","seq","brake_pct",\n"temp_c","distance_cm","sensor_ok"}'),
      ('alert','QoS 1 · sin retain · por evento','{"node_id","ts_ms","seq",\n"alert_type","severity"}'),
      ('status','QoS 1 · retenido · LWT','{"node_id","state":"online"}\nLWT → "offline" (~15 s)')]
ys=[5.3,3.3,1.3]
for (t,q,p),y in zip(rows,ys):
    box(3.4,y,3.0,1.3,f'v2v/{{node_id}}/{t}',11,True)
    box(6.9,y,3.4,1.3,q+'\n\n'+p,9)
    arr(2.6,4.0,3.4,y+0.65)
    arr(10.3,y+0.65,11.2,4.0)
box(11.2,3.0,2.2,2.0,'Mosquitto\n(Raspberry Pi 5)\nusuario y\ncontraseña',11,True)
arr(13.4,4.0,14.0,4.0)
box(14.0,3.0,1.9,2.0,'Node-RED\n(laptop)\nsuscrito\na v2v/#',11,True)
ax.text(8,0.35,'alert_type: manual | hard_brake | obstacle | overtemp      severity: info | warning | critical      sensor_ok: bits DIST 1 · TEMP 2 · BRAKE 4\n'
        'Contrato completo: interfaces/mqtt-topics.md y interfaces/payload.schema.json',ha='center',fontsize=10,style='italic')
fig.savefig('flujo_mqtt.png',dpi=150,bbox_inches='tight',facecolor='white'); fig.savefig('flujo_mqtt.svg',bbox_inches='tight',facecolor='white')
