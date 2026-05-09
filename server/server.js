const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const coap = require('coap');
const path = require('path');

const app = express();
const server = http.createServer(app);
const io = new Server(server);

// Cấu hình IP của ESP32 (LƯU Ý: Đổi thành IP DHCP thực tế in ra trên Serial Monitor nếu bị đổi)
const ESP32_IP = '192.168.2.79';

// Trỏ Express phục vụ các file tĩnh trong thư mục web
app.use(express.static(path.join(__dirname, '../web')));

// Polling: Lấy dữ liệu từ ESP32 mỗi 1 giây
setInterval(() => {
    const req = coap.request(`coap://${ESP32_IP}/state`);
    
    req.on('response', (res) => {
        try {
            const data = JSON.parse(res.payload.toString());
            // Đẩy dữ liệu realtime lên UI
            io.emit('sensor_data', data);
        } catch(e) {
            console.error("JSON Parse Error:", e.message);
        }
    });

    req.on('error', (err) => {
        console.error("CoAP Timeout/Error - ESP32 Offline?", err.message);
    });

    req.end();
}, 1000);

// Xử lý lệnh từ Web UI gửi xuống
io.on('connection', (socket) => {
    console.log('Một Client Web vừa kết nối!');

    socket.on('command', (cmd) => {
        // Gửi lệnh PUT xuống ESP32 qua CoAP
        const req = coap.request({
            hostname: ESP32_IP,
            pathname: '/command',
            method: 'PUT',
            confirmable: false // <--- ĐÃ THÊM: Tắt tính năng tự động gửi lại lệnh (Confirmable) để chống "lệnh ma"
        });
        req.write(cmd);
        req.end();
    });
});

const PORT = 3000;
server.listen(PORT, () => {
    console.log(`Backend Server đang chạy tại: http://localhost:${PORT}`);
});