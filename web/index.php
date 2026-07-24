<?php
/**
 * Modbus-IEC104 Gateway Web Dashboard
 * Main dashboard page
 */

session_start();

// Configuration
$CONFIG = [
    'gateway_socket' => '/tmp/gateway.sock',
    'api_endpoint' => 'http://localhost:8080/api',
    'refresh_interval' => 2000, // milliseconds
];

// Initialize session data
if (!isset($_SESSION['user_authenticated'])) {
    $_SESSION['user_authenticated'] = false;
}

// Check authentication
if (!$_SESSION['user_authenticated'] && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $username = $_POST['username'] ?? '';
    $password = $_POST['password'] ?? '';
    
    // Simple authentication (replace with proper auth)
    if ($username === 'admin' && $password === 'admin123') {
        $_SESSION['user_authenticated'] = true;
        $_SESSION['login_time'] = time();
    }
}

// API endpoint handler
if (isset($_GET['api'])) {
    handleAPI($CONFIG);
    exit;
}

// Login check redirect
if (!$_SESSION['user_authenticated']) {
    showLoginPage();
    exit;
}

?>
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Modbus-IEC104 Gateway Dashboard</title>
    <link rel="stylesheet" href="assets/css/style.css">
    <link rel="stylesheet" href="assets/css/dashboard.css">
    <script src="assets/js/chart.min.js"></script>
</head>
<body>
    <nav class="navbar">
        <div class="container">
            <div class="navbar-brand">
                <h1>🔗 Modbus-IEC104 Gateway</h1>
            </div>
            <div class="navbar-menu">
                <span class="user-info">Welcome, <?php echo htmlspecialchars($_SESSION['username'] ?? 'Admin'); ?></span>
                <a href="?logout" class="btn-logout">Logout</a>
            </div>
        </div>
    </nav>

    <div class="container">
        <!-- Status Bar -->
        <div class="status-bar">
            <div class="status-card">
                <div class="status-label">Gateway Status</div>
                <div class="status-value" id="gateway-status">
                    <span class="badge badge-loading">Checking...</span>
                </div>
            </div>
            <div class="status-card">
                <div class="status-label">Modbus Connected</div>
                <div class="status-value" id="modbus-status">
                    <span class="badge badge-unknown">Unknown</span>
                </div>
            </div>
            <div class="status-card">
                <div class="status-label">IEC104 Clients</div>
                <div class="status-value" id="iec104-clients">
                    <span class="badge">0</span>
                </div>
            </div>
            <div class="status-card">
                <div class="status-label">Uptime</div>
                <div class="status-value" id="gateway-uptime">--:--:--</div>
            </div>
        </div>

        <!-- Main Dashboard Grid -->
        <div class="dashboard-grid">
            <!-- Modbus Section -->
            <div class="card">
                <div class="card-header">
                    <h2>📊 Modbus Configuration</h2>
                    <button class="btn btn-sm btn-edit" onclick="showModbusConfig()">Edit</button>
                </div>
                <div class="card-body">
                    <form id="modbus-form" class="config-form">
                        <div class="form-group">
                            <label>Device IP</label>
                            <input type="text" id="modbus-ip" class="form-control" value="127.0.0.1" readonly>
                        </div>
                        <div class="form-group">
                            <label>Port</label>
                            <input type="number" id="modbus-port" class="form-control" value="502" readonly>
                        </div>
                        <div class="form-group">
                            <label>Slave ID</label>
                            <input type="number" id="modbus-slave" class="form-control" value="1" readonly>
                        </div>
                        <div class="form-group">
                            <label>Baud Rate</label>
                            <input type="number" id="modbus-baud" class="form-control" value="9600" readonly>
                        </div>
                    </form>
                    <div class="action-buttons">
                        <button class="btn btn-primary" onclick="testModbusConnection()">Test Connection</button>
                        <button class="btn btn-success" onclick="startModbus()">Start</button>
                        <button class="btn btn-danger" onclick="stopModbus()">Stop</button>
                    </div>
                </div>
            </div>

            <!-- IEC104 Section -->
            <div class="card">
                <div class="card-header">
                    <h2>🔌 IEC104 Configuration</h2>
                    <button class="btn btn-sm btn-edit" onclick="showIEC104Config()">Edit</button>
                </div>
                <div class="card-body">
                    <form id="iec104-form" class="config-form">
                        <div class="form-group">
                            <label>Listen Port</label>
                            <input type="number" id="iec104-port" class="form-control" value="2404" readonly>
                        </div>
                        <div class="form-group">
                            <label>Max Connections</label>
                            <input type="number" id="iec104-max-conn" class="form-control" value="10" readonly>
                        </div>
                        <div class="form-group">
                            <label>Timeout (seconds)</label>
                            <input type="number" id="iec104-timeout" class="form-control" value="30" readonly>
                        </div>
                    </form>
                    <div class="action-buttons">
                        <button class="btn btn-success" onclick="startIEC104()">Start Server</button>
                        <button class="btn btn-danger" onclick="stopIEC104()">Stop Server</button>
                    </div>
                </div>
            </div>

            <!-- Data Mapping -->
            <div class="card card-full-width">
                <div class="card-header">
                    <h2>🗺️ Data Mapping</h2>
                    <button class="btn btn-sm btn-success" onclick="showAddMappingModal()">+ Add Mapping</button>
                </div>
                <div class="card-body">
                    <table class="table" id="mapping-table">
                        <thead>
                            <tr>
                                <th>ID</th>
                                <th>Modbus Address</th>
                                <th>Type</th>
                                <th>IEC104 Object</th>
                                <th>Status</th>
                                <th>Actions</th>
                            </tr>
                        </thead>
                        <tbody id="mapping-tbody">
                            <tr class="loading">
                                <td colspan="6" style="text-align: center;">Loading mappings...</td>
                            </tr>
                        </tbody>
                    </table>
                </div>
            </div>

            <!-- Real-time Data -->
            <div class="card card-full-width">
                <div class="card-header">
                    <h2>📈 Real-time Data</h2>
                    <button class="btn btn-sm btn-secondary" onclick="toggleDataRefresh()">
                        <span id="refresh-toggle">Pause</span> Updates
                    </button>
                </div>
                <div class="card-body">
                    <div class="data-grid" id="data-grid">
                        <div class="data-item">
                            <div class="data-label">Modbus Reads</div>
                            <div class="data-value" id="stat-modbus-reads">0</div>
                        </div>
                        <div class="data-item">
                            <div class="data-label">Modbus Writes</div>
                            <div class="data-value" id="stat-modbus-writes">0</div>
                        </div>
                        <div class="data-item">
                            <div class="data-label">IEC104 Messages</div>
                            <div class="data-value" id="stat-iec104-msgs">0</div>
                        </div>
                        <div class="data-item">
                            <div class="data-label">Errors</div>
                            <div class="data-value error" id="stat-errors">0</div>
                        </div>
                    </div>

                    <canvas id="performanceChart" style="margin-top: 20px;"></canvas>
                </div>
            </div>

            <!-- Logs -->
            <div class="card card-full-width">
                <div class="card-header">
                    <h2>📝 Activity Log</h2>
                    <button class="btn btn-sm btn-secondary" onclick="clearLogs()">Clear</button>
                </div>
                <div class="card-body">
                    <div class="log-container" id="log-container">
                        <div class="log-entry">Waiting for activity...</div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Modals -->
    <div id="modbus-config-modal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3>Edit Modbus Configuration</h3>
                <button class="btn-close" onclick="closeModal('modbus-config-modal')">&times;</button>
            </div>
            <div class="modal-body">
                <form id="modbus-edit-form">
                    <div class="form-group">
                        <label>Device IP</label>
                        <input type="text" id="edit-modbus-ip" class="form-control" required>
                    </div>
                    <div class="form-group">
                        <label>Port</label>
                        <input type="number" id="edit-modbus-port" class="form-control" required>
                    </div>
                    <div class="form-group">
                        <label>Slave ID</label>
                        <input type="number" id="edit-modbus-slave" class="form-control" required>
                    </div>
                    <div class="form-group">
                        <label>Baud Rate</label>
                        <input type="number" id="edit-modbus-baud" class="form-control" required>
                    </div>
                </form>
            </div>
            <div class="modal-footer">
                <button class="btn btn-secondary" onclick="closeModal('modbus-config-modal')">Cancel</button>
                <button class="btn btn-primary" onclick="saveModbusConfig()">Save</button>
            </div>
        </div>
    </div>

    <div id="add-mapping-modal" class="modal">
        <div class="modal-content">
            <div class="modal-header">
                <h3>Add Data Mapping</h3>
                <button class="btn-close" onclick="closeModal('add-mapping-modal')">&times;</button>
            </div>
            <div class="modal-body">
                <form id="add-mapping-form">
                    <div class="form-group">
                        <label>Modbus Address</label>
                        <input type="text" id="mapping-modbus-addr" class="form-control" placeholder="e.g., 0x100" required>
                    </div>
                    <div class="form-group">
                        <label>Data Type</label>
                        <select id="mapping-type" class="form-control" required>
                            <option value="">Select Type</option>
                            <option value="int16">Int16</option>
                            <option value="uint16">UInt16</option>
                            <option value="int32">Int32</option>
                            <option value="uint32">UInt32</option>
                            <option value="float">Float</option>
                            <option value="bit">Bit</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label>IEC104 Object Number</label>
                        <input type="number" id="mapping-iec104-obj" class="form-control" required>
                    </div>
                    <div class="form-group">
                        <label>Scale Factor</label>
                        <input type="number" id="mapping-scale" class="form-control" value="1" step="0.01">
                    </div>
                </form>
            </div>
            <div class="modal-footer">
                <button class="btn btn-secondary" onclick="closeModal('add-mapping-modal')">Cancel</button>
                <button class="btn btn-primary" onclick="saveMapping()">Add Mapping</button>
            </div>
        </div>
    </div>

    <script src="assets/js/api.js"></script>
    <script src="assets/js/dashboard.js"></script>
</body>
</html>

<?php

function showLoginPage() {
    ?>
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Gateway Login</title>
        <link rel="stylesheet" href="assets/css/style.css">
        <style>
            body {
                display: flex;
                align-items: center;
                justify-content: center;
                height: 100vh;
                background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            }
            .login-container {
                background: white;
                padding: 40px;
                border-radius: 10px;
                box-shadow: 0 10px 40px rgba(0,0,0,0.2);
                width: 100%;
                max-width: 400px;
            }
            .login-container h1 {
                text-align: center;
                margin-bottom: 30px;
                color: #333;
            }
            .form-group {
                margin-bottom: 20px;
            }
            .form-group label {
                display: block;
                margin-bottom: 8px;
                color: #555;
                font-weight: 600;
            }
            .form-group input {
                width: 100%;
                padding: 12px;
                border: 1px solid #ddd;
                border-radius: 5px;
                font-size: 16px;
            }
            .btn-login {
                width: 100%;
                padding: 12px;
                background: #667eea;
                color: white;
                border: none;
                border-radius: 5px;
                font-size: 16px;
                font-weight: 600;
                cursor: pointer;
                transition: background 0.3s;
            }
            .btn-login:hover {
                background: #764ba2;
            }
        </style>
    </head>
    <body>
        <div class="login-container">
            <h1>🔗 Gateway Login</h1>
            <form method="POST">
                <div class="form-group">
                    <label>Username</label>
                    <input type="text" name="username" required>
                </div>
                <div class="form-group">
                    <label>Password</label>
                    <input type="password" name="password" required>
                </div>
                <button type="submit" class="btn-login">Login</button>
            </form>
            <p style="text-align: center; margin-top: 20px; color: #999; font-size: 12px;">
                Demo: admin / admin123
            </p>
        </div>
    </body>
    </html>
    <?php
}

function handleAPI($config) {
    header('Content-Type: application/json');
    
    $action = $_GET['action'] ?? '';
    
    switch ($action) {
        case 'status':
            echo json_encode(getGatewayStatus($config));
            break;
        
        case 'mappings':
            echo json_encode(getMappings($config));
            break;
        
        case 'stats':
            echo json_encode(getStats($config));
            break;
        
        case 'logs':
            echo json_encode(getLogs($config));
            break;
        
        default:
            echo json_encode(['error' => 'Unknown action']);
    }
}

function getGatewayStatus($config) {
    return [
        'status' => 'running',
        'uptime' => time() - (time() % 86400),
        'modbus_connected' => true,
        'iec104_clients' => 2,
        'version' => '1.0.0'
    ];
}

function getMappings($config) {
    return [
        ['id' => 1, 'modbus_addr' => '0x100', 'type' => 'int16', 'iec104_obj' => 1001, 'status' => 'active'],
        ['id' => 2, 'modbus_addr' => '0x101', 'type' => 'uint16', 'iec104_obj' => 1002, 'status' => 'active'],
        ['id' => 3, 'modbus_addr' => '0x102', 'type' => 'int32', 'iec104_obj' => 1003, 'status' => 'inactive'],
    ];
}

function getStats($config) {
    return [
        'modbus_reads' => 1523,
        'modbus_writes' => 342,
        'iec104_messages' => 8934,
        'errors' => 2
    ];
}

function getLogs($config) {
    return [
        ['timestamp' => date('Y-m-d H:i:s'), 'level' => 'INFO', 'message' => 'Gateway started'],
        ['timestamp' => date('Y-m-d H:i:s', time()-10), 'level' => 'INFO', 'message' => 'Modbus connection established'],
        ['timestamp' => date('Y-m-d H:i:s', time()-20), 'level' => 'WARNING', 'message' => 'IEC104 client connected from 192.168.1.100'],
    ];
}

// Logout handler
if (isset($_GET['logout'])) {
    session_destroy();
    header('Location: index.php');
    exit;
}
?>
