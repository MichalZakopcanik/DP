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
const socket = io(); //leaving empty because same origin, if different origin use io("http://yourserver:port")
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

let latestData = {};
let fetchedData = [];
let currentRange = "";
let cutoffDate;
let firstLoad = true;

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
    latestData = air_data;
    if (!firstLoad){
        updateGraphs();
    }
    firstLoad = false;
});

async function loadGraphs(range = "hour") {
    if (range === currentRange) return; //preventing fetching the same data again
    const response = await fetch(`/api/${range}`)
    if (response.ok) {
       fetchedData = await response.json();
       console.log("Current range: " + currentRange);
       console.log("Fetched range: " + range);
       console.log("Fetched data: " + fetchedData);
       currentRange = range;
       fetchedData = addBlankData(fetchedData);
       /*if (fetchedData && fetchedData.length > 0) {
        console.log("First timestamp: " + fetchedData[0].timestamp);
        console.log("Last timestamp: " + fetchedData[fetchedData.length-1].timestamp);
       }*/
       createGraphs(fetchedData, "create");     
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

function addBlankData(data) {
    if(!data || data.length == 0) return data;
    let timestamp = data.map(item => new Date(item.timestamp));
    let modifiedData = [];
    //console.log("Inside addBlankData");
    for (let i = 1; i < timestamp.length; i++) {
        let currentTime = new Date(timestamp[i]);
        let previousTime = new Date(timestamp[i - 1]);
        let timeDiff = (currentTime - previousTime) / (1000 * 60); //time difference in minutes
        //console.log("Time difference between " + timestamp[i-1] + " and " + timestamp[i] + ": " + timeDiff + " minutes");
        modifiedData.push(data[i-1]);
        if (timeDiff > 60){ //if above 1 hour, add blank data for each hour
            for (let j = timeDiff; j > 0; j-=60) {
                let diffTime = new Date(currentTime);
                diffTime.setMinutes(diffTime.getMinutes()-1*j);
                //console.log("Added diffTime " + diffTime);
                let blankData = {
                    timestamp: diffTime,
                    temperature: null,
                    humidity: null,
                    aqi: null,
                    co2: null,
                    tvoc: null,
                    pm1: null,
                    pm2: null,
                    pm10: null
                };
                modifiedData.push(blankData);
                //console.log("Added blank data at " + dateTimeFormat.format(blankData.timestamp));
            }
        } else if(timeDiff > 10){
            let blankData = {
                    timestamp: new Date(currentTime - timeDiff/2 * 60 * 1000),
                    temperature: null,
                    humidity: null,
                    aqi: null,
                    co2: null,
                    tvoc: null,
                    pm1: null,
                    pm2: null,
                    pm10: null
                };
            modifiedData.push(blankData);
        }
    }
    modifiedData.push(data[data.length-1]);
    return modifiedData;
}

function createGraphs(data, type = "create") {
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
        xaxis: {title: {text: 'Čas'}},
        yaxis: {title: {text:'Teplota (°C)'}},
        yaxis2: {
            title: {text:'Vlhkosť (%)'},
            overlaying: 'y',
            side: 'right'
        },
        margin: { l: 60, r: 80, t: 20, b: 250 },
        autosize: true
    };

    var graphData = [trace1, trace2];
    if (type === "create") Plotly.newPlot('graphDiv', graphData, layout); 
    else {Plotly.react('graphDiv', graphData, layout);}   

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
        xaxis: {title: {text:'Čas'}},
        yaxis: {title: {text:'PM1 (µg/m³)'}},
        yaxis2: {
            title: {text:'PM2.5 (µg/m³)'},
            overlaying: 'y',
            side: 'right'
        },
        yaxis3: {
            title: {text:'PM10 (µg/m³)'},
            overlaying: 'y',
            side: 'right',
            position: 0.95
        },
        margin: { l: 60, r: 80, t: 20, b: 250 },
        autosize: true
    };
    
    var graphData = [trace1, trace2, trace3];
    if (type === "create") Plotly.newPlot('graphDiv2', graphData, layout); 
    else {Plotly.react('graphDiv2', graphData, layout);} 

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
        xaxis: {title: {text:'Čas'}},
        yaxis: {title: {text:'AQI'}},
        yaxis2: {
            title: {text:'CO2 (ppm)'},
            overlaying: 'y',
            side: 'right'
        },
        yaxis3: {
            title: {text:'TVOC (ppb)'},
            overlaying: 'y',
            side: 'right',
            position: 0.95
        },
        margin: { l: 60, r: 80, t: 20, b: 250 },
        autosize: true
    };

    var graphData = [trace1, trace2, trace3];
    if (type === "create") Plotly.newPlot('graphDiv3', graphData, layout); 
    else {Plotly.react('graphDiv3', graphData, layout);} 
}

function updateGraphs() {
    const timestamp = fetchedData.map(item => new Date(item.timestamp));
    console.log(timestamp);
    cutoffDate = new Date();
    switch (currentRange){
        case "hour":
            cutoffDate.setHours(cutoffDate.getHours() - 1);
            break;
        case "day":
            cutoffDate.setDate(cutoffDate.getDate() - 1);
            break;
        case "week":
            cutoffDate.setDate(cutoffDate.getDate() - 7);
            break;
        case "month":
            cutoffDate.setMonth(cutoffDate.getMonth() - 1);
            break;
        case "6months":
            cutoffDate.setMonth(cutoffDate.getMonth() - 6);
            break;
        default:
            break;
    }
    if (timestamp.length > 0 && timestamp[0] < cutoffDate) {
        fetchedData = fetchedData.filter(item => new Date(item.timestamp) >= cutoffDate);
        createGraphs(fetchedData, "update");
    } 
    fetchedData.push(latestData);
    Plotly.extendTraces('graphDiv', {   x:[[dateTimeFormat.format(new Date(latestData.timestamp))], [dateTimeFormat.format(new Date(latestData.timestamp))]], 
                                        y: [[latestData.temperature], [latestData.humidity]]}, 
                                        [0,1]);
    Plotly.extendTraces('graphDiv2', {  x:[[dateTimeFormat.format(new Date(latestData.timestamp))], [dateTimeFormat.format(new Date(latestData.timestamp))], [dateTimeFormat.format(new Date(latestData.timestamp))]], 
                                        y: [[latestData.pm1], [latestData.pm2], [latestData.pm10]]}, 
                                        [0,1,2]);
    Plotly.extendTraces('graphDiv3', {  x:[[dateTimeFormat.format(new Date(latestData.timestamp))], [dateTimeFormat.format(new Date(latestData.timestamp))], [dateTimeFormat.format(new Date(latestData.timestamp))]], 
                                        y: [[latestData.aqi], [latestData.co2], [latestData.tvoc]]}, 
                                        [0,1,2]);
}

loadGraphs(); //initial load - last hour data