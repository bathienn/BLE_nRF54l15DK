#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/services/bas.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ble_advertise, LOG_LEVEL_INF);

volatile bool ble_ready = false;

static uint32_t custom_data[] = {1203, 123, 53214};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, custom_data, sizeof(custom_data)),
};

uint64_t received_data[100];

static void on_write(struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    const void *buf,
    uint16_t len,
    uint16_t offset,
    uint8_t flags)
{
    const uint8_t *data = buf;
    LOG_INF("Received %d bytes:\n", len);
    for (int i = 0; i < len; i++) {
        LOG_INF("0x%02X ", data[i]);
    }
    LOG_INF("\n");
}

static uint8_t read_value[] = {0x01, 0x02, 0x03, 0x05};

static ssize_t on_read(struct bt_conn *conn, const struct bt_gatt_attr *attr, 
    void *buf, uint16_t len, uint16_t offset)
{
    // Trả về dữ liệu (read_value) khi có yêu cầu đọc
    size_t read_len = sizeof(read_value);

    // Nếu offset lớn hơn kích thước dữ liệu, không có gì để trả về
    if (offset > read_len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    // Trả về phần dữ liệu từ offset
    return bt_gatt_attr_read(conn, attr, buf, len, offset, read_value, read_len);
}

static struct bt_gatt_attr *tx_attr;

void send_notify_data(struct bt_conn *conn)
{
    uint8_t msg[16];
    int err;
    err = bt_gatt_notify(conn, tx_attr, msg, sizeof(msg));
    if (err) {
        LOG_ERR("Notify msg failed (err %d)", err);
    } else {
        LOG_INF("Notify msg ok");
    }
    for (int time = 0; time < 20; time++){
        for (int i=0 ; i < 16; i++){
            msg[i] = rand() % 256;
        }
        err = bt_gatt_notify(conn, tx_attr, msg, sizeof(msg));
        if (err) {
            LOG_ERR("Notify msg failed (err %d)", err);
            return;
        } else {
            LOG_INF("Notify %d ok", time);
        }
        k_msleep(500);
    }
}

static struct bt_conn *default_conn;
static void tx_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == BT_GATT_CCC_NOTIFY) {
        LOG_INF("Notify enabled");
        send_notify_data(default_conn);
    }
}

BT_GATT_SERVICE_DEFINE(my_service,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_16(0x1234)),  // Battery service, có thể thay đổi tùy ý
    BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_16(0x9876),   // UUID của characteristic
    BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,            // Cho phép ghi vào
    BT_GATT_PERM_WRITE,           // Quyền ghi
    on_read, on_write, NULL),        // Callback khi nhận dữ liệu
    BT_GATT_CCC(tx_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
); 

void bt_ready(int err)
{
    if (err) {
        LOG_ERR("BLE enable failed (err %d)", err);
        return;
    }
    LOG_INF("BLE stack ready");
    ble_ready = true;
}

int init_ble(void)
{
    LOG_INF("Initializing BLE...");
    int err = bt_enable(bt_ready);
    if (err) {
        LOG_ERR("bt_enable failed (err %d)", err);
        return err;
    }
    return 0;
}

// static struct bt_conn *default_conn;
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    default_conn = bt_conn_ref(conn);
    LOG_INF("Device connected");
}

// Callback khi thiết bị ngắt kết nối
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Device disconnected (reason %u)", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

// Đăng ký callback
static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

int main(void)
{
    init_ble();
    while(!ble_ready){
        LOG_INF("BLE stack not ready");
        k_msleep(10);
    }
    LOG_INF("BLE ready");

    bt_conn_cb_register(&conn_callbacks);

    tx_attr = &my_service.attrs[2];

    int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err){
        LOG_INF("advertiseing faile to start");
        return;
    }

    LOG_INF("advertising done");
    while(1){
        k_msleep(100);
    }
}
