import mqtt from 'mqtt';
import express from 'express';
import { Server } from "socket.io";
import * as http from 'http';
import Database from 'better-sqlite3';

const app = express();
const server = http.createServer(app); //http server
const io = new Server(server); //socketIO server
const db = new Database('air_data.db');
db.pragma('journal_mode = WAL');//WAL for concurrent read/write access

const MQTT_IP = 'EXAMPLE_IP'; //replace with MQTT broker IP address
const MQTT_PORT = 'EXAMPLE_PORT'; //replace with MQTT broker port
const MQTT_URL = `mqtt://${MQTT_IP}:${MQTT_PORT}`;
let client;

//to store latest data in memory
let currentData = {
  timestamp: null,
  millisTimestamp: null,
  temperature: null,
  humidity: null,
  pm1: null,
  pm2: null,
  pm10: null,
  co2: null,
  aqi: null,
  tvoc: null
};

function initializeDatabase() {
  return new Promise((resolve, reject) => {
    try {
      const stmt = db.prepare(`CREATE TABLE IF NOT EXISTS airQ_data(
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
        millisTimestamp INTEGER,
        temperature REAL,
        humidity REAL,
        pm1 REAL,
        pm2 REAL,
        pm10 REAL,
        co2 REAL,
        aqi REAL,
        tvoc REAL);`);
      stmt.run();
      resolve();
    } catch (error) {
      console.error("Couldnt create the table:", error);
      reject(error);
    }
  });
};

function dbInsert() {
  const stmt = db.prepare(`INSERT INTO airQ_data (
    timestamp,
    millisTimestamp,
    temperature,
    humidity,
    pm1,
    pm2,
    pm10,
    co2,
    aqi,
    tvoc) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`);
  stmt.run(
  currentData.timestamp,
  currentData.millisTimestamp,
  currentData.temperature,
  currentData.humidity,
  currentData.pm1,
  currentData.pm2,
  currentData.pm10,
  currentData.co2,
  currentData.aqi,
  currentData.tvoc
);
}

function startServer() {
  server.listen(3000);
  client = mqtt.connect(MQTT_URL);
  client.on("connect", () => {
    client.subscribe("/example/data", {qos:1}, (err) => {
      if (!err) {
        console.log(`Subscribed to /example/data topic`);
      } else {
        console.error(`Failed to subscribe to /example/data topic:`, err);
      }
    });
  });

  client.on("message", (topic, message, packet) => {
    console.log(`${topic}: ${message.toString()}, retained: ${packet.retain}, qos: ${packet.qos}`);

    if (topic === "/example/data") {
      const data = JSON.parse(message.toString());
      let timestamp = new Date().toISOString(); //default - expecting new data, thus using current time
      if (!packet.retain){
        dbInsert();
      } else {  //if message retained on broker, get latest timestamp from DB
        const stmt = db.prepare(`SELECT timestamp FROM airQ_data ORDER BY id DESC LIMIT 1`);
        const row = stmt.get();
        timestamp = row.timestamp;
      }
      currentData = { ...currentData, ...data, timestamp: timestamp };
      io.emit('air-update', currentData);
    } else {
      console.warn(`Received message on unknown topic: ${topic}`);
    }

  });
}

app.use(express.static('public'));

//API endpoint for latest data
app.get('/api/current', (req, res) => {
  res.json(currentData);
});

//API endpoint for historical data
app.get('/api/:range', (req, res) => {
  const range = req.params.range;
  let timeLimit;
  
  switch(range) {
    case 'hour':
      timeLimit = new Date(Date.now() - 60 * 60 * 1000);
      break;
    case 'day':
      timeLimit = new Date(Date.now() - 24 * 60 * 60 * 1000);
      break;
    case 'week':
      timeLimit = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000);
      break;
    case 'month':
      timeLimit = new Date(Date.now() - 30 * 24 * 60 * 60 * 1000);
      break;
    case '6months':
      timeLimit = new Date(Date.now() - 180 * 24 * 60 * 60 * 1000);
      break;
    default:
      timeLimit = new Date(Date.now() - 60 * 60 * 1000);
  }
  
  const stmt = db.prepare(
    `SELECT timestamp, temperature, humidity, pm1, pm2, pm10, co2, aqi, tvoc 
     FROM airQ_data 
     WHERE timestamp > ? 
     ORDER BY timestamp ASC`);
  const rows = stmt.all(timeLimit.toISOString());
  res.json(rows);
});

initializeDatabase().then(() => {
  startServer();
  console.log('Server is listening on port 3000');
}).catch((error) => {
  console.error("Failed to initialize database:", error);
  process.exit(1);
});

io.on('connection', (socket) => {
  console.log('A user connected');
  socket.emit('air-update', currentData);
  socket.on('disconnect', () => {
    console.log('A user disconnected');
  });
});

function shutdown() {
  db.close();
  if (client) client.end();
  io.close();
  console.log('Resources have been cleaned up.');
}

process.on('SIGINT', () => {
  console.log('Received SIGINT. Shutting down gracefully...');
  shutdown();
  process.exit(128 + 2);
});