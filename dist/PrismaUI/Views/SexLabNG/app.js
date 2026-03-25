const INVOKE_METHOD = "SexLabNG.GetCurrentActorEnjoyment";

const enjoymentEl = document.getElementById("enjoymentValue");
const degreeEl = document.getElementById("degreeValue");
const statusEl = document.getElementById("statusText");
const refreshBtn = document.getElementById("refreshBtn");

function clampEnjoyment(value) {
    if (Number.isNaN(value)) {
        return 0;
    }
    return Math.max(-100, Math.min(100, value));
}

function setEnjoymentClass(value) {
    enjoymentEl.classList.remove("metric__value--blue", "metric__value--red", "metric__value--neutral");

    if (value < 0) {
        enjoymentEl.classList.add("metric__value--blue");
        return;
    }

    if (value > 0) {
        enjoymentEl.classList.add("metric__value--red");
        return;
    }

    enjoymentEl.classList.add("metric__value--neutral");
}

async function fetchActorData() {
    if (!window.prisma || typeof window.prisma.invoke !== "function") {
        // 在浏览器直接打开文件时，提供回退示例，便于调样式。
        return {
            enjoyment: 0,
            degree: 0,
            _mock: true
        };
    }

    // 约定 SKSE 返回: { enjoyment: number, degree: number }
    return window.prisma.invoke(INVOKE_METHOD);
}

function renderData(data) {
    const enjoyment = clampEnjoyment(Number(data?.enjoyment));
    const degree = Number.isFinite(Number(data?.degree)) ? Number(data.degree) : 0;

    enjoymentEl.textContent = String(enjoyment);
    degreeEl.textContent = String(degree);
    setEnjoymentClass(enjoyment);
}

async function refresh() {
    statusEl.textContent = "正在拉取数据...";

    try {
        const data = await fetchActorData();
        renderData(data);

        if (data?._mock) {
            statusEl.textContent = "当前为浏览器预览模式（未接入 SKSE）";
        } else {
            statusEl.textContent = "数据已更新";
        }
    } catch (error) {
        statusEl.textContent = `获取失败: ${error?.message || String(error)}`;
    }
}

refreshBtn.addEventListener("click", refresh);
window.addEventListener("DOMContentLoaded", refresh);
