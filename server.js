const express = require('express');
const { MongoClient } = require('mongodb');
const bodyParser = require('body-parser');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const uri = "mongodb+srv://sheikhabdulbaseer786:yoBFfmoORB3QEpjM@cluster0.hiywggw.mongodb.net/?retryWrites=true&w=majority&appName=Cluster0";
const client = new MongoClient(uri);

let db;

async function connectToMongo() {
  if (!db) {
    try {
      await client.connect();
      db = client.db("DHT11");
      console.log("✅ Connected to MongoDB");
    } catch (error) {
      console.error("❌ MongoDB connection failed:", error);
      throw error;
    }
  }
}

app.use(bodyParser.json());
app.use(cors());
app.use(express.static(path.join(__dirname, 'public')));

// POST endpoint to receive load cell data
console.log("Incoming request body:", req.body);
app.post('/api/loadcell', async (req, res) => {
  const { weight, percentage } = req.body;

  if (weight == null || percentage == null) {
    return res.status(400).send("Missing weight or percentage");
  }

  try {
    await connectToMongo();
    const collection = db.collection("weight_readings");

    const newReading = {
      weight,
      percentage,
      timestamp: new Date(new Date().toLocaleString("en-US", { timeZone: "Asia/Karachi" })),
    };

    await collection.insertOne(newReading);
    res.status(200).send("Weight reading stored successfully");
  } catch (err) {
    console.error("Error saving to DB:", err);
    res.status(500).send("Error saving to database");
  }
});

// GET endpoint to fetch all readings as JSON
app.get('/api/loadcell/readings', async (req, res) => {
  try {
    await connectToMongo();
    const collection = db.collection("weight_readings");

    const readings = await collection.find().toArray();
    res.json(readings);
  } catch (err) {
    console.error("Error fetching data:", err);
    res.status(500).send("Error fetching data from database");
  }
});

// Start the server
app.listen(PORT, () => {
  console.log(`📦 Weight server running at http://localhost:${PORT}`);
});
