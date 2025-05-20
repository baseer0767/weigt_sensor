const express = require('express');
const { MongoClient } = require('mongodb');
const bodyParser = require('body-parser');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const uri = "mongodb://localhost:27017";
const client = new MongoClient(uri);

app.use(bodyParser.json());
app.use(cors());

// Serve static files from the public folder
app.use(express.static(path.join(__dirname, 'public')));

// POST endpoint to receive load cell data
app.post('/api/loadcell', async (req, res) => {
  const { weight, percentage } = req.body;

  if (weight == null || percentage == null) {
    return res.status(400).send("Missing weight or percentage");
  }

  try {
    await client.connect();
    const db = client.db("DHT11");
    const collection = db.collection("weight_readings");

    const newReading = {
      weight: weight, // Store as received
      percentage: percentage, // Store as received
      timestamp: new Date(new Date().toLocaleString("en-US", { timeZone: "Asia/Karachi" }))
    };

    await collection.insertOne(newReading);
    res.status(200).send("Weight reading stored successfully");
  } catch (err) {
    console.error(err);
    res.status(500).send("Error saving to database");
  } finally {
    await client.close();
  }
});

// New API endpoint to get all readings as JSON
app.get('/api/loadcell/readings', async (req, res) => {
  try {
    await client.connect();
    const db = client.db("DHT11");
    const collection = db.collection("weight_readings");

    const readings = await collection.find().toArray();
    res.json(readings);
  } catch (err) {
    console.error(err);
    res.status(500).send("Error fetching data from database");
  } finally {
    await client.close();
  }
});

// Start server
app.listen(PORT, () => {
  console.log(`📦 Weight server running at http://localhost:${PORT}`);
});
