# app.py
# FastAPI + Pandas + Plotly + ROS 2 (rclpy)

import threading
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from collections import deque
import json

from fastapi import FastAPI
from fastapi.responses import HTMLResponse, JSONResponse
import pandas as pd
import numpy as np
import plotly.graph_objs as go

app = FastAPI()

# ==============================================================================
# 1. INTEGRAÇÃO ROS 2
# ==============================================================================

MAX_POINTS = 400
data_buffer = deque(maxlen=MAX_POINTS)
start_time = None

class OdomListener(Node):
    def __init__(self):
        super().__init__('interface_fisica_web_listener')
        self.subscription = self.create_subscription(
            Odometry, '/odom', self.listener_callback, 10)

    def listener_callback(self, msg):
        global start_time
        now = msg.header.stamp.sec + msg.header.stamp.nanosec * 1e-9
        if start_time is None: start_time = now
        
        data_buffer.append({
            't': float(now - start_time),
            'x': float(msg.pose.pose.position.x),
            'y': float(msg.pose.pose.position.y)
        })

def start_ros():
    rclpy.init()
    node = OdomListener()
    try: rclpy.spin(node)
    except: pass
    finally: 
        if rclpy.ok(): rclpy.shutdown()

threading.Thread(target=start_ros, daemon=True).start()

# ==============================================================================
# 2. CÁLCULOS FÍSICOS
# ==============================================================================

def get_processed_data():
    if len(data_buffer) < 5:
        return {'t': [], 'x': [], 'y': [], 'v': [], 'a': [], 'R': []}

    df = pd.DataFrame(list(data_buffer))
    
    # Filtro de Média Móvel
    window = 10
    df['x_s'] = df['x'].rolling(window=window, min_periods=1).mean()
    df['y_s'] = df['y'].rolling(window=window, min_periods=1).mean()

    dx = np.gradient(df['x_s'])
    dy = np.gradient(df['y_s'])
    dt = np.gradient(df['t'])
    dt[dt == 0] = 0.001
    
    v = np.sqrt(dx**2 + dy**2) / dt
    v_smooth = pd.Series(v).rolling(window=5, min_periods=1).mean().fillna(0).values

    a = np.gradient(v_smooth) / dt
    a_smooth = pd.Series(a).rolling(window=10, min_periods=1).mean().fillna(0).values

    ddx = np.gradient(dx)
    ddy = np.gradient(dy)
    denom = (dx**2 + dy**2)**1.5
    denom[denom < 0.0001] = 0.0001
    k = np.abs(dx * ddy - dy * ddx) / denom
    
    R = np.zeros_like(k)
    mask = k > 0.001
    R[mask] = 1 / k[mask]
    R[R > 50] = 50

    return {
        't': df['t'].tolist(),
        'x': df['x'].tolist(),
        'y': df['y'].tolist(),
        'xs': df['x_s'].tolist(),
        'ys': df['y_s'].tolist(),
        'v': v_smooth.tolist(),
        'a': a_smooth.tolist(),
        'R': R.tolist()
    }

@app.get("/data_json")
def data_endpoint():
    return JSONResponse(get_processed_data())

# ==============================================================================
# 3. INTERFACE
# ==============================================================================

@app.get("/", response_class=HTMLResponse)
def index():
    return """
    <h1>Análise Cinemática do Carrinho</h1>
    <ul>
      <li><a href='/mru'>MRU / MRUV</a></li>
      <li><a href='/rotacao'>Rotação – Raio da Curva</a></li>
      <li><a href='/frenagem'>Frenagem</a></li>
    </ul>
    """

@app.get("/mru", response_class=HTMLResponse)
def mru():
    fig1 = go.Figure()
    fig1.add_trace(go.Scatter(x=[], y=[], name='x(t)'))
    fig1.add_trace(go.Scatter(x=[], y=[], name='y(t)'))
    fig1.update_layout(title='Posição × Tempo')

    fig2 = go.Figure()
    fig2.add_trace(go.Scatter(x=[], y=[], name='v(t)'))
    fig2.update_layout(title='Velocidade × Tempo')

    fig3 = go.Figure()
    fig3.add_trace(go.Scatter(x=[], y=[], name='a(t)'))
    fig3.update_layout(title='Aceleração × Tempo')

    script_update = """
    <script>
        setInterval(async () => {
            const res = await fetch('/data_json');
            const d = await res.json();
            if(d.t.length === 0) return;

            Plotly.react('g1', [{x: d.t, y: d.x, name:'x(t)'}, {x: d.t, y: d.y, name:'y(t)'}], {title: 'Posição × Tempo'});
            Plotly.react('g2', [{x: d.t, y: d.v, name:'v(t)'}], {title: 'Velocidade × Tempo'});
            Plotly.react('g3', [{x: d.t, y: d.a, name:'a(t)'}], {title: 'Aceleração × Tempo'});
        }, 500);
    </script>
    """
    
    # Texto simples acima de cada gráfico
    return """<h3>Posição x Tempo</h3>""" + \
           fig1.to_html(div_id='g1', include_plotlyjs='cdn', full_html=False) + \
           """<h3>Velocidade x Tempo</h3>""" + \
           fig2.to_html(div_id='g2', include_plotlyjs=False, full_html=False) + \
           """<h3>Aceleração x Tempo</h3>""" + \
           fig3.to_html(div_id='g3', include_plotlyjs=False, full_html=False) + \
           script_update

@app.get("/rotacao", response_class=HTMLResponse)
def rotacao():
    fig1 = go.Figure()
    fig1.add_trace(go.Scatter(x=[], y=[], mode='lines', name='Trajetória'))
    fig1.update_layout(title='Trajetória do Carrinho', xaxis_title='x', yaxis_title='y')

    fig2 = go.Figure()
    fig2.add_trace(go.Scatter(x=[], y=[], name='Raio da Curvatura'))
    fig2.update_layout(title='Raio da Curva × Tempo')

    script_update = """
    <script>
        setInterval(async () => {
            const res = await fetch('/data_json');
            const d = await res.json();
            if(d.t.length === 0) return;

            Plotly.react('g1', [{x: d.xs, y: d.ys, mode:'lines', name:'Trajetória'}], {title: 'Trajetória do Carrinho', xaxis: {title:'x'}, yaxis: {title:'y'}});
            Plotly.react('g2', [{x: d.t, y: d.R, name:'Raio da Curvatura'}], {title: 'Raio da Curva × Tempo'});
        }, 500);
    </script>
    """

    return """<h3>Trajetória (XY)</h3>""" + \
           fig1.to_html(div_id='g1', include_plotlyjs='cdn', full_html=False) + \
           """<h3>Raio de Curvatura x Tempo</h3>""" + \
           fig2.to_html(div_id='g2', include_plotlyjs=False, full_html=False) + \
           script_update

@app.get("/frenagem", response_class=HTMLResponse)
def frenagem():
    fig1 = go.Figure()
    fig1.add_trace(go.Scatter(x=[], y=[], name='Velocidade'))
    fig1.update_layout(title='Velocidade × Tempo (Frenagem)')

    fig2 = go.Figure()
    fig2.add_trace(go.Scatter(x=[], y=[], name='Desaceleração'))
    fig2.update_layout(title='Desaceleração × Tempo')

    script_update = """
    <script>
        setInterval(async () => {
            const res = await fetch('/data_json');
            const d = await res.json();
            if(d.t.length === 0) return;

            Plotly.react('g1', [{x: d.t, y: d.v, name:'Velocidade'}], {title: 'Velocidade × Tempo (Frenagem)'});
            Plotly.react('g2', [{x: d.t, y: d.a, name:'Desaceleração'}], {title: 'Desaceleração × Tempo'});
        }, 500);
    </script>
    """

    return """<h3>Velocidade x Tempo</h3>""" + \
           fig1.to_html(div_id='g1', include_plotlyjs='cdn', full_html=False) + \
           """<h3>Desaceleração x Tempo</h3>""" + \
           fig2.to_html(div_id='g2', include_plotlyjs=False, full_html=False) + \
           script_update
