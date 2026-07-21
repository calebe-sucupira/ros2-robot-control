const page = document.body.dataset.page;

const pageCharts = {
    mru: [
        {
            title: "Posição × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Posição (m)",
            series: [
                {key: "x", name: "x(t)", color: "#62d4ff"},
                {key: "y", name: "y(t)", color: "#a98bff"},
            ],
        },
        {
            title: "Velocidade × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Velocidade (m/s)",
            series: [
                {key: "v", name: "v(t)", color: "#66e3a4"},
            ],
        },
        {
            title: "Aceleração × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Aceleração (m/s²)",
            series: [
                {key: "a", name: "a(t)", color: "#ffba69"},
            ],
        },
    ],
    rotacao: [
        {
            title: "Trajetória no plano",
            x: "xs",
            xLabel: "x (m)",
            yLabel: "y (m)",
            equalAxes: true,
            series: [
                {
                    key: "ys",
                    name: "Trajetória",
                    color: "#62d4ff",
                },
            ],
        },
        {
            title: "Raio de curvatura × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Raio estimado (m)",
            series: [
                {
                    key: "R",
                    name: "Raio",
                    color: "#a98bff",
                },
            ],
        },
    ],
    frenagem: [
        {
            title: "Velocidade × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Velocidade (m/s)",
            series: [
                {
                    key: "v",
                    name: "Velocidade",
                    color: "#62d4ff",
                },
            ],
        },
        {
            title: "Desaceleração × tempo",
            x: "t",
            xLabel: "Tempo (s)",
            yLabel: "Desaceleração (m/s²)",
            series: [
                {
                    key: "deceleration",
                    name: "Desaceleração",
                    color: "#ff7b8b",
                },
            ],
        },
    ],
};

const charts = pageCharts[page] ?? [];
const chartsElement = document.querySelector("#charts");
const statusDot = document.querySelector("#status-dot");
const statusText = document.querySelector("#status-text");

function createChartContainers() {
    charts.forEach((chart, index) => {
        const article = document.createElement("article");
        article.className = "chart-card";

        const heading = document.createElement("h2");
        heading.textContent = chart.title;

        const plot = document.createElement("div");
        plot.id = `chart-${index}`;
        plot.className = "plot";

        article.append(heading, plot);
        chartsElement.append(article);
    });
}

function chartLayout(chart) {
    return {
        margin: {t: 24, r: 24, b: 56, l: 64},
        paper_bgcolor: "transparent",
        plot_bgcolor: "transparent",
        font: {color: "#c9d4e5"},
        legend: {orientation: "h", y: 1.12},
        xaxis: {
            title: chart.xLabel,
            gridcolor: "#253247",
            zerolinecolor: "#35445c",
        },
        yaxis: {
            title: chart.yLabel,
            gridcolor: "#253247",
            zerolinecolor: "#35445c",
            ...(chart.equalAxes
                ? {scaleanchor: "x", scaleratio: 1}
                : {}),
        },
    };
}

function updateCharts(data) {
    charts.forEach((chart, index) => {
        const traces = chart.series.map((series) => ({
            x: data[chart.x],
            y: data[series.key],
            name: series.name,
            type: "scatter",
            mode: "lines",
            line: {
                color: series.color,
                width: 3,
            },
        }));

        Plotly.react(
            `chart-${index}`,
            traces,
            chartLayout(chart),
            {
                responsive: true,
                displaylogo: false,
            },
        );
    });
}

function updateMetric(id, value) {
    document.querySelector(id).textContent =
        Number.isFinite(value) ? value.toFixed(3) : "—";
}

function updateMetrics(data) {
    const last = data.t.length - 1;

    updateMetric("#metric-time", data.t[last]);
    updateMetric("#metric-speed", data.v[last]);
    updateMetric("#metric-acceleration", data.a[last]);
    updateMetric("#metric-radius", data.R[last]);
}

function setStatus(rosAvailable, connected) {
    statusDot.classList.toggle("connected", connected);
    statusDot.classList.toggle("unavailable", !rosAvailable);

    if (!rosAvailable) {
        statusText.textContent = "ROS 2 indisponível";
    } else {
        statusText.textContent = connected
            ? "Recebendo odometria"
            : "Aguardando dados";
    }
}

async function refresh() {
    try {
        const response = await fetch("/data_json", {
            cache: "no-store",
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}`);
        }

        const data = await response.json();
        const hasData = data.t.length > 0;

        setStatus(data.ros_available, hasData);

        if (hasData) {
            updateCharts(data);
            updateMetrics(data);
        }
    } catch {
        setStatus(false, false);
    }
}

createChartContainers();
refresh();
setInterval(refresh, 500);
