// Theme toggle logic
let darkmode = localStorage.getItem("darkmode");
const themeToggleButton = document.getElementById("theme-toggle-btn");

const enableDarkMode = () => {  
    document.body.classList.add("darkmode");
    localStorage.setItem("darkmode", "active");
}

const disableDarkMode = () => {
    document.body.classList.remove("darkmode");
    localStorage.setItem("darkmode", null);
}

if (darkmode === "active") enableDarkMode();

themeToggleButton.addEventListener("click", () => {
    darkmode = localStorage.getItem("darkmode");
    if (darkmode !== "active") {
        enableDarkMode();
    } else {
        disableDarkMode();
    }
});

// Socket.IO setup
const socket = io("EXAMPLE_IP:EXAMPLE_PORT"); //replace with your server IP and port
const timestampLastCard = document.getElementById("timestamp");
const temperatureLastCard = document.getElementById("temperature");
const humidityLastCard = document.getElementById("humidity");
const aqiLastCard = document.getElementById("aqi");
const pm1LastCard = document.getElementById("pm1");
const pm2LastCard = document.getElementById("pm2");
const pm10LastCard = document.getElementById("pm10");
const co2LastCard = document.getElementById("co2");
const tvocLastCard = document.getElementById("tvoc");
const dateTimeFormat = new Intl.DateTimeFormat("sk", 
    {   timeZone: "Europe/Bratislava",
        year: "numeric",
        month: "2-digit",
        day: "2-digit",
        hour: "2-digit",
        minute: "2-digit",
        second: "2-digit"
    }); //formatting timestamp to specific locale and timezone

let sessionData = [];

socket.on("air-update", (air_data) => {
    if (air_data.timestamp != null) {
        timestampLastCard.innerText = `Posledné meranie: ${dateTimeFormat.format(new Date(air_data.timestamp))}`;
    }
    if (air_data.temperature != null) {
        temperatureLastCard.innerText = `Temperature: ${air_data.temperature} °C`;
    }
    if (air_data.humidity != null) {
        humidityLastCard.innerText = `Humidity: ${air_data.humidity} %`;
    }
    if (air_data.aqi != null) {
        aqiLastCard.innerText = `AQI: ${air_data.aqi}`;
    }
    if (air_data.pm1 != null) {
        pm1LastCard.innerText = `PM1: ${air_data.pm1} µg/m³`;
    }
    if (air_data.pm2 != null) {
        pm2LastCard.innerText = `PM2.5: ${air_data.pm2} µg/m³`;
    }
    if (air_data.pm10 != null) {
        pm10LastCard.innerText = `PM10: ${air_data.pm10} µg/m³`;
    }
    if (air_data.co2 != null) {
        co2LastCard.innerText = `CO2: ${air_data.co2} ppm`;
    }
    if (air_data.tvoc != null) {
        tvocLastCard.innerText = `TVOC: ${air_data.tvoc} ppb`;
    }
});

async function loadGraphs(range = "hour") {
    const response = await fetch(`/api/${range}`)
    if (response.ok) {
       const data = await response.json();
       createGraphs(data);     
    }
    else {
        throw new Error(`Failed to fetch historical data: ${response.status} ${response.statusText}`);
    } 
};

const graphButtonHour = document.getElementById("graph-btn-hour");
const graphButtonDay = document.getElementById("graph-btn-day");
const graphButtonWeek = document.getElementById("graph-btn-week");
const graphButtonMonth = document.getElementById("graph-btn-month");
const graphButton6Months = document.getElementById("graph-btn-6months");


graphButtonHour.addEventListener("click", () => {
    loadGraphs('hour');
});

graphButtonDay.addEventListener("click", () => {
    loadGraphs('day');
});

graphButtonWeek.addEventListener("click", () => {
    loadGraphs('week');
});

graphButtonMonth.addEventListener("click", () => {
    loadGraphs('month');
});

graphButton6Months.addEventListener("click", () => {
    loadGraphs('6months');
});

//using newPlot for simplicity
//TODO: optimize for better performance
function createGraphs(data) {
    const timestamp = data.map(item => dateTimeFormat.format(new Date(item.timestamp)));
    const temperature = data.map(item => item.temperature);
    const humidity = data.map(item => item.humidity);
    const aqi = data.map(item => item.aqi);
    const co2 = data.map(item => item.co2);
    const tvoc = data.map(item => item.tvoc);
    const pm1 = data.map(item => item.pm1);
    const pm2 = data.map(item => item.pm2);
    const pm10 = data.map(item => item.pm10);
    
    var trace1 = {
    x: timestamp,
    y: temperature,
    mode: 'lines',
    name: 'Teplota (°C)',
    yaxis: 'y'
    };

    var trace2 = {
    x: timestamp,
    y: humidity,
    mode: 'lines',
    name: 'Vlhkosť (%)',
    yaxis: 'y2'
    };

    var layout = {
        title: 'Teplota a vlhkosť',
        xaxis: {title: 'Čas'},
        yaxis: {title: 'Teplota (°C)'},
        yaxis2: {
            title: 'Vlhkosť (%)',
            overlaying: 'y',
            side: 'right'
        }
    };

    var graphData = [trace1, trace2];
    Plotly.newPlot('graphDiv', graphData, layout);    

    var trace1 = {
    x: timestamp,
    y: pm1,
    mode: 'lines',
    name: 'PM1 (µg/m³)',
    yaxis: 'y'
    };

    var trace2 = {
    x: timestamp,
    y: pm2,
    mode: 'lines',
    name: 'PM2.5 (µg/m³)',
    yaxis: 'y2'
    };

    var trace3 = {
    x: timestamp,
    y: pm10,
    mode: 'lines',
    name: 'PM10 (µg/m³)',
    yaxis: 'y3'
    };

    var layout = {
        title: 'PM1, PM2.5 a PM10',
        xaxis: {title: 'Čas'},
        yaxis: {title: 'PM1 (µg/m³)'},
        yaxis2: {
            title: 'PM2.5 (µg/m³)',
            overlaying: 'y',
            side: 'right'
        },
        yaxis3: {
            title: 'PM10 (µg/m³)',
            overlaying: 'y',
            side: 'right',
            position: 0.95
        }
    };
    
    var graphData = [trace1, trace2, trace3];
    Plotly.newPlot('graphDiv2', graphData, layout);

    var trace1 = {
    x: timestamp,
    y: aqi,
    mode: 'lines',
    name: 'AQI',
    yaxis: 'y'
    };

    var trace2 = {
    x: timestamp,
    y: co2,
    mode: 'lines',
    name: 'CO2 (ppm)',
    yaxis: 'y2'
    };

    var trace3 = {
    x: timestamp,
    y: tvoc,
    mode: 'lines',
    name: 'TVOC (ppb)',
    yaxis: 'y3'
    };
    
    var layout = {
        title: 'AQI, CO2 a TVOC',
        xaxis: {title: 'Čas'},
        yaxis: {title: 'AQI'},
        yaxis2: {
            title: 'CO2 (ppm)',
            overlaying: 'y',
            side: 'right'
        },
        yaxis3: {
            title: 'TVOC (ppb)',
            overlaying: 'y',
            side: 'right',
            position: 0.95
        }
    };

    var graphData = [trace1, trace2, trace3];
    Plotly.newPlot('graphDiv3', graphData, layout);
}

loadGraphs(); //initial load of graphs - last hour data