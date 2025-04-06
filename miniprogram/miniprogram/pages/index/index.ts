Page({
    data: {
        // 说明书面板标志位
        collapse_val: [0],
        // 设备属性
        temperature: 0.0,
        humidity: 0.0,
        led_state: false,
        ble: [0],

        led_on_off: false,
        isRealTime: false, // 是否为实时数据
        mqtt_state: '设备状态: 未连接', // mqtt连接状态
        ble_state: '设备状态: 未连接', // ble连接状态

        buttonTheme: 'default',  // 按钮主题，默认灰色
        buttonText: '未获取认证',  // 按钮文本，默认显示“未获取认证”
        // URL 配置
        tokenUrl: 'https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens',
        shadowUrl: 'https://ed6cc26730.st1.iotda-app.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/shadow',
        commandUrl: 'https://ed6cc26730.st1.iotda-app.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/commands',
        deviceUrl: 'https://ed6cc26730.st1.iotda-app.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32',

        projectId: 'cn-north-4',
        deviceId: '67ed58015367f573f77ef961_esp32',
        serviceId: 'gateway_data',
        commandName: 'ctrl',

        authDomain: "odddouglas",
        authUser: "iota",
        authPassword: "qgddgls1128"
    },

    onLoad() {
        console.log("页面 onLoad");
        this.getToken();
    },
    onShow() {
        console.log("页面 onShow");
        this.checkDeviceStatus();
    },
    onHide() {
        clearInterval(this.timer);
    },
    onUnload() {
        clearInterval(this.timer);
    },

    handleChange1(e) {
        this.setData({ led_on_off: e.detail.value });
        this.setCommand();
    },
    handleChange2(e) {
        this.setData({ collapse_val: e.detail.value });
    },
    // 获取 token 并认证成功后进行判断
    handleButton1() {
        this.getToken();
    },

    getToken() {
        const token = wx.getStorageSync('token');
        if (token) {
            this.setData({
                buttonTheme: 'primary',  // 按钮变蓝
                buttonText: '认证成功',  // 显示认证成功文本
            });
            wx.showToast({ title: '已经完成认证，无需程重复验证', icon: 'none', duration: 2000 });
            return;
        } //如果有了就无需重复获取，区别于其他需要token的函数
        wx.request({
            url: this.data.tokenUrl,
            method: 'POST',
            data: JSON.stringify({
                auth: {
                    identity: {
                        methods: ["password"],
                        password: {
                            user: {
                                domain: { name: this.data.authDomain },
                                name: this.data.authUser,
                                password: this.data.authPassword
                            }
                        }
                    },
                    scope: {
                        domain: {},
                        project: { name: this.data.projectId }
                    }
                }
            }),
            header: { 'Content-Type': 'application/json' },
            success: (res) => {
                const token = res.header['X-Subject-Token'];
                wx.setStorageSync('token', token);
                this.setData({
                    buttonTheme: 'primary',  // 按钮变蓝
                    buttonText: '认证成功',  // 显示认证成功文本
                });
                // 获取设备在线状态
                this.checkDeviceStatus();
            },
            fail() {
                wx.showToast({ title: '认证失败', icon: 'none', duration: 2000 });
            },
            complete() {
                console.log("获取token完成");
            }
        });
    },

    // 检查设备是否在线
    checkDeviceStatus() {
        const token = wx.getStorageSync('token');
        if (!token) return;
        wx.request({
            url: this.data.deviceUrl,
            method: 'GET',
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success: (res) => {
                const status = res.data.status;
                console.log("设备状态:", status);
                if (status === "ONLINE") {
                    this.setData({
                        isRealTime: true,
                        mqtt_state: '设备状态: 在线'
                    });
                    wx.showToast({ title: '设备在线', icon: 'success', duration: 1500 });

                    // 启动定时获取影子
                    clearInterval(this.timer);
                    this.timer = setInterval(() => {
                        this.getShadow();
                    }, 500);
                } else {
                    this.setData({
                        isRealTime: false,
                        mqtt_state: '设备状态: 离线'
                    });
                    wx.showToast({ title: '设备不在线，仅显示设备离线前最后一次数据', icon: 'none', duration: 2000 });
                    this.getShadow(); // 获取一次影子，作为上次在线数据
                }
            },
            fail() {
                wx.showToast({ title: '状态查询失败', icon: 'none', duration: 2000 });
            }
        });
    },

    getShadow() {
        const token = wx.getStorageSync('token');
        if (!token) return;

        wx.request({
            url: this.data.shadowUrl,
            method: 'GET',
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success: (res) => {
                const props = res.data.shadow[0]?.reported?.properties || {};
                console.log(props); //打印数据
                this.setData({
                    temperature: props.temperature || 0,
                    humidity: props.humidity || 0,
                    led_state: props.led || false,
                    ble: props.ble || [],
                    ble_state: (props.ble[0] === "true")
                        ? `设备状态: 已连接 (${props.ble[1] || 'BLE设备'})`
                        : '设备状态: 未连接'
                });
            }
        });
    },

    setCommand() {
        const token = wx.getStorageSync('token');
        if (!token) {
            wx.showToast({ title: '请先获取认证', icon: 'none', duration: 2000 });
            return;
        }

        wx.request({
            url: this.data.commandUrl,
            method: 'POST',
            data: JSON.stringify({
                service_id: this.data.serviceId,
                command_name: this.data.commandName,
                paras: { led_on_off: this.data.led_on_off }
            }),
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success(res) {
                if (res.statusCode === 403 && res.data?.error_code === "IOTDA.014016") {
                    wx.showToast({ title: '设备不在线', icon: 'none', duration: 2000 });
                    return;
                }
                wx.showToast({ title: '命令下发成功', icon: 'success', duration: 1000 });
            },
            fail() {
                wx.showToast({ title: '命令发送失败', icon: 'none', duration: 2000 });
            }
        });
    }
});


//获取设备属性（通过设备影子）
//GET https://{endpoint}/v5/iot/{project_id}/devices/{device_id}/shadow

//下发设备命令 同获取设备影子的API类似，也是利用HTTP完成对应URL的相应请求，然后解析响应数据即可，
// POST https://{endpoint}/v5/iot/{project_id}/devices/{device_id}/commands

//终端节点Endpoint
//iotda.cn-north-4.myhuaweicloud.com

//获取token
//https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens 


