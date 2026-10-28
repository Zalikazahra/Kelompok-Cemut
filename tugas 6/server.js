const express = require('express');
const mysql = require('mysql2');
const cors = require('cors');

const app = express();
app.use(express.json());
app.use(cors());

// Koneksi Database
const db = mysql.createConnection({
    host: 'localhost',
    user: 'root',
    password: '',
    database: 'absensi_gps',
    port: 3307
});
db.connect(err => {
    if (err) {
        console.error('Gagal koneksi database:', err.message);
    } else {
        console.log('Database terhubung...');
    }
});

// Endpoint 1: API Login
app.post('/api/login', (req, res) => {
    const { username, password } = req.body;
    const query = 'SELECT id, username, full_name, role FROM users WHERE username = ? AND password = ?';

    db.query(query, [username, password], (err, results) => {
        if (err) {
            console.error('SERVER ERROR:', err);
            return res.status(500).json({ success: false, message: 'Server Error' });
        }
        
        if (results.length > 0) {
            console.log(`[LOGIN BERHASIL] User: "${results[0].username}" (${results[0].full_name}) berhasil masuk.`);
            res.json({ success: true, user: results[0] });
        } else {
            console.warn(`[LOGIN GAGAL] Percobaan login gagal untuk username: "${username}"`);
            res.status(401).json({ success: false, message: 'Username atau Password salah' });
        }
    });
});

// Endpoint 2: API Submit Absen GPS
app.post('/api/attendance', (req, res) => {
    const { userId, latitude, longitude } = req.body;
    const checkInTime = new Date();

    const query = 'INSERT INTO attendance (user_id, latitude, longitude, check_in_time) VALUES (?, ?, ?, ?)';
    db.query(query, [userId, latitude, longitude, checkInTime], (err, result) => {
        if (err) {
            console.error('ABSEN ERROR:', err);
            return res.status(500).json({ success: false, message: 'Gagal mencatat absensi' });
        }
        
        res.json({
            success: true,
            message: 'Absen berhasil dicatat',
            data: { time: checkInTime, latitude, longitude }
        });
    });
});

app.listen(3000, () => {
    console.log('Server berjalan pada http://localhost:3000');
});