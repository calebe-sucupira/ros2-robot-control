from contextlib import asynccontextmanager
from pathlib import Path
from threading import Event, Thread

from fastapi import FastAPI, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

from dashboard.physics import process_samples
from dashboard.ros_listener import (
    ROS_AVAILABLE,
    TelemetryBuffer,
    run_ros,
)

BASE_DIR = Path(__file__).resolve().parent

telemetry = TelemetryBuffer(max_points=400)
stop_event = Event()


@asynccontextmanager
async def lifespan(_: FastAPI):
    ros_thread = Thread(
        target=run_ros,
        args=(telemetry, stop_event),
        daemon=True,
        name="ros-odometry-listener",
    )
    ros_thread.start()

    yield

    stop_event.set()
    ros_thread.join(timeout=2)


app = FastAPI(
    title="Análise Cinemática do Carrinho",
    lifespan=lifespan,
)

app.mount(
    "/static",
    StaticFiles(directory=BASE_DIR / "static"),
    name="static",
)

templates = Jinja2Templates(directory=BASE_DIR / "templates")

PAGES = {
    "mru": {
        "title": "Movimento e aceleração",
        "description": "Posição, velocidade e aceleração ao longo do tempo.",
    },
    "rotacao": {
        "title": "Rotação e trajetória",
        "description": "Trajetória no plano e raio estimado de curvatura.",
    },
    "frenagem": {
        "title": "Análise de frenagem",
        "description": "Velocidade e desaceleração durante a frenagem.",
    },
}


def render_analysis(request: Request, page: str):
    page_data = PAGES[page]

    return templates.TemplateResponse(
        "analysis.html",
        {
            "request": request,
            "page": page,
            "title": page_data["title"],
            "description": page_data["description"],
        },
    )


@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse(
        "index.html",
        {
            "request": request,
            "pages": PAGES,
        },
    )


@app.get("/mru", response_class=HTMLResponse)
def mru(request: Request):
    return render_analysis(request, "mru")


@app.get("/rotacao", response_class=HTMLResponse)
def rotacao(request: Request):
    return render_analysis(request, "rotacao")


@app.get("/frenagem", response_class=HTMLResponse)
def frenagem(request: Request):
    return render_analysis(request, "frenagem")


@app.get("/data_json")
def data_endpoint():
    data = process_samples(telemetry.snapshot())
    data["ros_available"] = ROS_AVAILABLE
    return data
