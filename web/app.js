const socket = io();

// Cấu hình Chart.js
const ctx = document.getElementById('gasChart').getContext('2d');
const gasChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Nồng độ Khí Gas (MQ2)',
            data: [],
            borderColor: '#e74c3c',
            backgroundColor: 'rgba(231, 76, 60, 0.15)', 
            borderWidth: 2,
            pointRadius: 3,
            tension: 0.3, 
            fill: true
        }]
    },
    options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
            y: { 
                beginAtZero: true, 
                max: 1500 
            } 
        },
        plugins: {
            legend: {
                labels: { color: '#2c3e50' } 
            }
        }
    }
});

const fsmDictionary = {
    0: "SAFE (An Toàn)",
    1: "GAS DETECTED (Phát hiện khí)",
    2: "ALARM (Báo Động)",
    3: "FAN RUNNING (Đang hút khí)"
};

socket.on('sensor_data', (data) => {
    document.getElementById('gasValue').innerText = data.gas;
    document.getElementById('fsmState').innerText = fsmDictionary[data.state];
    document.getElementById('modeValue').innerText = data.mode;

    const fsmCard = document.querySelector('.card.fsm');
    if (data.state === 0) fsmCard.style.borderBottom = "5px solid #2ecc71";
    else fsmCard.style.borderBottom = "5px solid #e74c3c";

    const now = new Date();
    const timeString = `${now.getHours()}:${now.getMinutes()}:${now.getSeconds()}`;
    
    if(gasChart.data.labels.length > 20) {
        gasChart.data.labels.shift();
        gasChart.data.datasets[0].data.shift();
    }
    
    gasChart.data.labels.push(timeString);
    gasChart.data.datasets[0].data.push(data.gas);
    gasChart.update();
});

// Hàm gửi lệnh chung xuống Server
function sendCommand(commandString) {
    socket.emit('command', commandString);
}

// --- LOGIC ĐIỀU KHIỂN QUẠT THEO API MỚI ---

// Xử lý nút Bật/Tắt Quạt
function toggleFanPower(state) {
    if (state === 1) {
        sendCommand('FAN_ON');
    } else {
        sendCommand('FAN_OFF');
    }
}

// Xử lý thanh trượt cài đặt tốc độ
function changeTargetSpeed(value) {
    // Chỉ gửi tốc độ cài đặt đi. ESP32 sẽ tự biết phải làm gì 
    // (nếu quạt đang bật thì update ngay, nếu đang tắt thì chỉ lưu vào RAM)
    sendCommand(`FAN_SPD:${value}`);
}