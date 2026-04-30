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
    for(let [key, value] of Object.entries(latestData)){
        checkValueColor(value, key);
        switch(key){
            case "aqi":
                aqiLastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "aqi");
                break;
            case "co2":
                co2LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "co2");
                break;
            case "tvoc":
                tvocLastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "tvoc");
                break;
            case "pm10":
                pm10LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "pm10");
                break;
            case "pm2":
                pm2LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "pm2");
                break;
            case "pm1":
                pm1LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(value, "pm1");
                break;
            default:
                break;
        }
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
let firstLoad = true;

function checkValueColor(dataValue, type){
    darkmode = localStorage.getItem("darkmode");
    switch(type){
        case "aqi":
            if (dataValue === 1) return darkmode === "active" ? "#00ad34" : "#0a7800";
            else if (dataValue === 2) return darkmode === "active" ? "#d8ca01" : "#c7ba00";
            else if (dataValue === 3) return darkmode === "active" ? "#ffa500" : "#ff8c00";
            else if (dataValue === 4) return darkmode === "active" ? "#ff4500" : "#ff6347";
            else if (dataValue === 5) return darkmode === "active" ? "#800080" : "#9932cc";
            else return darkmode === "active" ? "#ffffff" : "#000000";
        case "co2":
            if (dataValue <= 1000) return darkmode === "active" ? "#00ad34" : "#0a7800";
            else if (dataValue > 1000 && dataValue <= 2000) return darkmode === "active" ? "#d8ca01" : "#c7ba00";
            else if (dataValue > 2000) return darkmode === "active" ? "#ff4500" : "#ff6347";
            else return "black";
        case "tvoc":
            if (dataValue < 150) return darkmode === "active" ? "#00ad34" : "#0a7800";
            else if (dataValue >= 150 && dataValue <= 500) return darkmode === "active" ? "#d8ca01" : "#c7ba00";
            else if (dataValue > 500 && dataValue <= 1500) return darkmode === "active" ? "#ffa500" : "#ff8c00";
            else if (dataValue > 1500 && dataValue <= 5000) return darkmode === "active" ? "#ff4500" : "#ff6347";
            else if (dataValue > 5000) return darkmode === "active" ? "#800080" : "#9932cc";
            else return darkmode === "active" ? "#ffffff" : "#000000";
        case "pm10":
            if (dataValue <= 20) return darkmode === "active" ? "#00ad34" : "#0a7800";
            else if (dataValue > 20 && dataValue <= 40) return darkmode === "active" ? "#d8ca01" : "#c7ba00";
            else if (dataValue > 40 && dataValue <= 100) return darkmode === "active" ? "#ffa500" : "#ff8c00";
            else if (dataValue > 100 && dataValue <= 180) return darkmode === "active" ? "#ff4500" : "#ff6347";
            else if (dataValue > 180) return darkmode === "active" ? "#800080" : "#9932cc";
            else return darkmode === "active" ? "#ffffff" : "#000000";
        case "pm2":
        case "pm1":
            if (dataValue <= 14) return darkmode === "active" ? "#00ad34" : "#0a7800";
            else if (dataValue > 14 && dataValue <= 25) return darkmode === "active" ? "#d8ca01" : "#c7ba00";
            else if (dataValue > 25 && dataValue <= 70) return darkmode === "active" ? "#ffa500" : "#ff8c00";
            else if (dataValue > 70 && dataValue <= 140) return darkmode === "active" ? "#ff4500" : "#ff6347";
            else if (dataValue > 140) return darkmode === "active" ? "#800080" : "#9932cc";
            else return darkmode === "active" ? "#ffffff" : "#000000";
        default:
            return darkmode === "active" ? "#ffffff" : "#000000";
    }
}

socket.on("air-update", (air_data) => {
    if (air_data.timestamp != null) {
        timestampLastCard.getElementsByClassName("card-value")[0].innerText = `${dateTimeFormat.format(new Date(air_data.timestamp))}`;
    }
    if (air_data.temperature != null) {
        temperatureLastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.temperature} °C`;
    }
    if (air_data.humidity != null) {
        humidityLastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.humidity} %`;
    }
    if (air_data.aqi != null) {
        aqiLastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.aqi}`;
        aqiLastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.aqi, "aqi");
    }
    if (air_data.pm1 != null) {
        pm1LastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.pm1} µg/m³`;
        pm1LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.pm1, "pm1");
    }
    if (air_data.pm2 != null) {
        pm2LastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.pm2} µg/m³`;
        pm2LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.pm2, "pm2");
    }
    if (air_data.pm10 != null) {
        pm10LastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.pm10} µg/m³`;
        pm10LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.pm10, "pm10");
    }
    if (air_data.co2 != null) {
        co2LastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.co2} ppm`;
        co2LastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.co2, "co2");
    }
    if (air_data.tvoc != null) {
        tvocLastCard.getElementsByClassName("card-value")[0].innerText = `${air_data.tvoc} ppb`;
        tvocLastCard.getElementsByClassName("card-value")[0].style.color = checkValueColor(air_data.tvoc, "tvoc");
    }
    latestData = air_data;
});