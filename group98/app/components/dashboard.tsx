
"use client";

import React, { useState, useEffect } from "react";
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from "recharts";

interface SensorData {
  temperature: number;
  humidity: number;
  light: number;
  timestamp: string;
}

const Dashboard: React.FC = () => {
  const [data, setData] = useState<SensorData[]>([]);
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    const fetchData = async () => {
      try {
        const response = await fetch("http://18.191.225.179:5000/data");
        const newData = await response.json();
        setData(newData);
      } catch (error) {
        console.error("Error fetching data:", error);
      }
    };

    fetchData();
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  const sendCommand = async (command: "open" | "close") => {
    setLoading(true);
    try {
      console.log(`Attempting to send ${command} command`);
      const response = await fetch("http://18.191.225.179:5000/control", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
          "Accept": "application/json"
        },
        body: JSON.stringify({ command })
      });
      
      const result = await response.json();
      console.log('Command response:', result);
      console.log('sup')
      if (result.command === command) {
        alert(`Successfully sent ${command} command!`);
      }
    } catch (err) {
      console.error('Error:', err);
      alert('Failed to send command');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="w-full max-w-6xl mx-auto">
      <div className="bg-white rounded-lg shadow-lg p-6">
        <h2 className="text-2xl font-bold text-center mb-6">
          Smart Blinds Dashboard
        </h2>

        <div className="h-[500px] w-full mb-6">
          <ResponsiveContainer width="100%" height="100%">
            <LineChart data={data}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis
                dataKey="timestamp"
                tick={{ fontSize: 12 }}
                angle={-45}
                textAnchor="end"
              />
              <YAxis
                yAxisId="temp"
                domain={[20, 30]}
                label={{
                  value: "Temperature (°C)",
                  angle: -90,
                  position: "insideLeft",
                }}
              />
              <YAxis
                yAxisId="humidity"
                orientation="right"
                domain={[40, 80]}
                label={{
                  value: "Humidity (%)",
                  angle: 90,
                  position: "insideRight",
                }}
              />
              <YAxis
                yAxisId="light"
                orientation="left"
                domain={[0, 1000]}
                label={{
                  value: "Light Level (lux)",
                  angle: -90,
                  position: "insideLeft",
                }}
              />
              <Tooltip />
              <Legend />
              <Line
                yAxisId="temp"
                type="monotone"
                dataKey="temperature"
                stroke="#ff9800"
                name="Temperature (°C)"
                dot={false}
              />
              <Line
                yAxisId="humidity"
                type="monotone"
                dataKey="humidity"
                stroke="#2196f3"
                name="Humidity (%)"
                dot={false}
              />
              <Line
                yAxisId="light"
                type="monotone"
                dataKey="light"
                stroke="#4caf50"
                name="Light Level (lux)"
                dot={false}
              />
            </LineChart>
          </ResponsiveContainer>
        </div>

        <div className="flex justify-center gap-4">
          <button
            className={`px-6 py-2 bg-green-500 text-white rounded-lg font-semibold ${
              loading ? "opacity-50 cursor-not-allowed" : "hover:bg-green-600"
            }`}
            onClick={() => sendCommand("open")}
            disabled={loading}
          >
            Open Blinds
          </button>
          <button
            className={`px-6 py-2 bg-red-500 text-white rounded-lg font-semibold ${
              loading ? "opacity-50 cursor-not-allowed" : "hover:bg-red-600"
            }`}
            onClick={() => sendCommand("close")}
            disabled={loading}
          >
            Close Blinds
          </button>
        </div>
      </div>
    </div>
  );
};

export default Dashboard;