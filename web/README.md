# Modbus-IEC104 Gateway Web Interface

A modern, responsive PHP web dashboard for managing the Modbus-IEC104 Gateway.

## Features

- **Real-time Status Monitoring**
  - Gateway status indicator
  - Modbus connection status
  - IEC104 client connections
  - System uptime display

- **Configuration Management**
  - Modbus device settings (IP, Port, Slave ID, Baud Rate)
  - IEC104 server settings (Listen Port, Max Connections, Timeout)
  - Live configuration updates

- **Data Mapping**
  - Create/edit/delete Modbus to IEC104 mappings
  - Support for multiple data types (Int16, UInt16, Int32, UInt32, Float, Bit)
  - Scale factor configuration

- **Real-time Data Monitoring**
  - Live statistics dashboard
  - Performance charts with Chart.js
  - Historical data tracking
  - Activity logs

- **Device Control**
  - Start/Stop Modbus connections
  - Start/Stop IEC104 server
  - Test Modbus connectivity
  - Clear activity logs

- **Security**
  - Session-based authentication
  - Simple login system (default: admin/admin123)
  - HTTPS ready (configure with Lighttpd)

## Installation on ARM64

### Prerequisites

- PHP 7.4+ with CLI
- Lighttpd web server
- SSL certificates (for HTTPS)

### Step 1: Install Lighttpd and PHP

```bash
# Update package manager
sudo apt-get update

# Install Lighttpd
sudo apt-get install -y lighttpd lighttpd-mod-fastcgi

# Install PHP-FPM
sudo apt-get install -y php-fpm php-cli php-json php-xml
```

### Step 2: Deploy Web Interface

```bash
# Create web root directory
sudo mkdir -p /var/www/gateway
cd /var/www/gateway

# Copy files from repository
git clone https://github.com/Uday1692/modbus-iec104-gateway.git
cp -r modbus-iec104-gateway/web/* /var/www/gateway/

# Set proper permissions
sudo chown -R www-data:www-data /var/www/gateway
sudo chmod -R 755 /var/www/gateway
```

### Step 3: Configure Lighttpd

Edit `/etc/lighttpd/lighttpd.conf`:

```conf
server.modules = (
    "mod_access",
    "mod_alias",
    "mod_auth",
    "mod_fastcgi",
    "mod_rewrite"
)

server.document-root = "/var/www/gateway"
server.port = 8080
server.username = "www-data"
server.groupname = "www-data"

# Enable FastCGI for PHP
fastcgi.server = ( ".php" =>
    (( "socket" => "/run/php/php-fpm.sock",
       "broken-scriptfilename" => "enable"
    ))
)

# HTTPS Configuration (Optional)
# $SERVER["socket"] == "0.0.0.0:8443" {
#     ssl.engine = "enable"
#     ssl.pemfile = "/etc/lighttpd/certs/gateway.pem"
#     server.document-root = "/var/www/gateway"
# }
```

### Step 4: Configure PHP-FPM

Edit `/etc/php/*/fpm/pool.d/www.conf`:

```conf
listen = /run/php/php-fpm.sock
listen.owner = www-data
listen.group = www-data
listen.mode = 0660
```

### Step 5: Enable HTTPS (Optional but Recommended)

```bash
# Generate self-signed certificate
sudo openssl req -x509 -nodes -days 365 -newkey rsa:2048 \
    -keyout /etc/lighttpd/certs/gateway.key \
    -out /etc/lighttpd/certs/gateway.crt

# Combine into PEM file
sudo cat /etc/lighttpd/certs/gateway.crt /etc/lighttpd/certs/gateway.key > \
    /etc/lighttpd/certs/gateway.pem

# Set permissions
sudo chmod 600 /etc/lighttpd/certs/gateway.pem
```

### Step 6: Start Services

```bash
# Start PHP-FPM
sudo systemctl start php*-fpm
sudo systemctl enable php*-fpm

# Start Lighttpd
sudo systemctl start lighttpd
sudo systemctl enable lighttpd

# Verify status
sudo systemctl status lighttpd
```

### Step 7: Access the Dashboard

- **HTTP**: `http://localhost:8080`
- **HTTPS** (if configured): `https://localhost:8443`
- **Default Login**: admin / admin123

## File Structure

```
web/
├── index.php                 # Main dashboard page
├── api.php                   # API endpoints (optional)
├── assets/
│   ├── css/
│   │   ├── style.css        # Main stylesheet
│   │   └── dashboard.css    # Dashboard-specific styles
│   ├── js/
│   │   ├── api.js           # API communication module
│   │   ├── dashboard.js     # Dashboard interactions
│   │   └── chart.min.js     # Chart.js library
│   └── img/
│       └── favicon.ico      # Website icon
└── config/
    └── database.example.php # Database config template
```

## API Endpoints

The dashboard communicates with the gateway via API endpoints:

- `?api=true&action=status` - Get gateway status
- `?api=true&action=stats` - Get statistics
- `?api=true&action=mappings` - Get data mappings
- `?api=true&action=logs` - Get activity logs
- `?api=true&action=modbus_start` - Start Modbus (POST)
- `?api=true&action=modbus_stop` - Stop Modbus (POST)
- `?api=true&action=iec104_start` - Start IEC104 (POST)
- `?api=true&action=iec104_stop` - Stop IEC104 (POST)
- `?api=true&action=add_mapping` - Add mapping (POST)
- `?api=true&action=delete_mapping&id=ID` - Delete mapping (DELETE)

## Troubleshooting

### 502 Bad Gateway Error
- Check if PHP-FPM is running: `sudo systemctl status php*-fpm`
- Verify socket path matches in Lighttpd config
- Check PHP-FPM socket permissions

### Permission Denied
```bash
sudo chown -R www-data:www-data /var/www/gateway
sudo chmod -R 755 /var/www/gateway
sudo chmod 644 /var/www/gateway/*.php
```

### HTTPS Certificate Issues
```bash
# Test certificate
openssl x509 -in /etc/lighttpd/certs/gateway.crt -text -noout

# Regenerate if needed
sudo rm /etc/lighttpd/certs/gateway.*
# Repeat certificate generation steps
```

## Security Notes

1. **Change Default Credentials**: Edit the authentication section in `index.php`
2. **Use HTTPS**: Always use SSL/TLS in production
3. **Firewall**: Restrict access to trusted networks
4. **Session Security**: Configure session timeout and cookie settings
5. **Input Validation**: Add proper input validation for production use

## Performance Optimization

### Lighttpd Configuration for Production

```conf
# Increase server limits
server.max-connections = 1000
server.max-fds = 2000

# Connection handling
connection.idle-timeout = 60
connection.read-timeout = 300

# Compression
compress.cache-dir = "/var/cache/lighttpd"
compress.filetype = ("text/plain", "text/html", "text/javascript", "text/css")
```

### PHP-FPM Tuning

```conf
pm = dynamic
pm.max_children = 32
pm.start_servers = 4
pm.min_spare_servers = 2
pm.max_spare_servers = 8
pm.max_requests = 1000
```

## Support

For issues and feature requests, visit:
- GitHub Issues: https://github.com/Uday1692/modbus-iec104-gateway/issues
- Documentation: https://github.com/Uday1692/modbus-iec104-gateway/wiki

## License

This project is licensed under the MIT License - see LICENSE file for details.
