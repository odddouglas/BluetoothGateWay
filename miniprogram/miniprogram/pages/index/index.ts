Page({
    data: {
        // 说明书面板标志位
        collapse_val: [0],
        // 多选栏选择属性
        checkbox_val: [0, 1],
        // 设备属性
        temperature: 0.0,
        humidity: 0.0,
        led: [0],
        ble: [0],

        led_on_off: ["false", "false", "false", "false"],
        ble_on_off: true,

        mqtt_on_off_line: false, // 是否为实时数据
        mqttStatus: '未连接到云', // mqtt连接状态
        ble_on_off_line: false,
        bleStatus: '离线', // ble连接状态

        buttonTheme: 'default',  // 按钮主题，默认灰色
        buttonText: '未获取认证',  // 按钮文本，默认显示“未获取认证”
        // URL 配置
        tokenUrl: 'https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens',
        shadowUrl: 'https://e5e7404266.st1.iotda-app.cn-north-4.myhuaweicloud.com:443/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/shadow',
        commandUrl: 'https://e5e7404266.st1.iotda-app.cn-north-4.myhuaweicloud.com:443/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/commands',
        deviceUrl: 'https://e5e7404266.st1.iotda-app.cn-north-4.myhuaweicloud.com:443/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32',

        // shadowUrl: 'https://iotda.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/shadow',
        // commandUrl: 'https://iotda.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/commands',
        // deviceUrl: 'https://iotda.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32',
        projectName: 'cn-north-4',
        projectId: '5631b5e6a3a34c86bc2e1cbd09ae9fc9',
        deviceId: '67ed58015367f573f77ef961_esp32',
        serviceId: 'gateway_data',
        commandName: 'ctrl',

        authDomain: "odddouglas",
        authUser: "iota",
        authPassword: "qgddgls1128",
        currentToken: null // 新增字段存储当前token
    },

    onLoad() {
        console.log("页面 onLoad");
        this.getToken();
        //console.log(wx.getStorageSync('token'));
    },
    onShow() {
        console.log("页面 onShow");
        // 启动定时检查设备状态
        this.timer = setInterval(() => {
            this.checkMQTTStatus();
            if (this.data.mqtt_on_off_line) {
                this.getShadow();
            }
        }, 3000); // 每5秒检查一次设备状态
    },
    onHide() {
        clearInterval(this.timer);
    },
    onUnload() {
        clearInterval(this.timer);
    },

    // handleChange1(e) {
    //     this.setData({ led_on_off: e.detail.value });
    // },

    // handleChange2(e) {
    //     this.setData({ ble_on_off: e.detail.value });
    // },
    handleChange3(e) {
        this.setData({ collapse_val: e.detail.value });
        console.log(this.data.checkbox_val);
    },
    handleChange4(e) {
        const selectedValues = e.detail.value; // 选中的 checkbox index
        this.setData({ checkbox_val: selectedValues });

        // 生成 led_on_off 状态（stringlist），共 4 位
        const led_on_off = [0, 1, 2, 3].map(i =>
            selectedValues.includes(i) ? "true" : "false"
        );

        this.setData({
            led_on_off: led_on_off
        });

        console.log("checkbox_val:", this.data.checkbox_val);
        console.log("led_on_off:", this.data.led_on_off);
    },


    // 获取 token 并认证成功后进行判断
    handleButton1() {
        this.getToken();
    },
    handleButton2() {
        this.setCommand();
    },
    handleCell1() {
        wx.showToast({ title: '检查网关连接情况，尽量保持wifi通畅', icon: 'none', duration: 1000 });
    },
    handleCell2() {
        wx.showToast({ title: '检查节点是否上电', icon: 'none', duration: 1000 });
    },
    getToken() {
        //const token = wx.getStorageSync('token');
        // if (token) {
        //     this.setData({
        //         buttonTheme: 'primary',  // 按钮变蓝
        //         buttonText: '认证成功',  // 显示认证成功文本
        //     });
        //     wx.showToast({ title: '已经完成认证，无需程重复验证', icon: 'none', duration: 2000 });
        //     return;
        // } //如果有了就无需重复获取，区别于其他需要token的函数
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
                        project: { name: this.data.projectName }
                    }
                }
            }),
            header: { 'Content-Type': 'application/json' },
            success: (res) => {
                const token = res.header['X-Subject-Token'];
                console.log(token);
                //wx.setStorageSync('token', token);
                this.setData({
                    buttonTheme: 'primary',  // 按钮变蓝
                    buttonText: '认证成功',  // 显示认证成功文本
                    currentToken: token  // 将token存储在data中而不是缓存
                });
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
    checkMQTTStatus() {
        //const token = wx.getStorageSync('token');
        const token = this.data.currentToken; // 改为从data中获取
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
                        mqtt_on_off_line: true,
                        mqttStatus: '连接到云'
                    });
                    // wx.showToast({ title: '设备在线', icon: 'success', duration: 1500 });
                } else {
                    this.setData({
                        mqtt_on_off_line: false,
                        mqttStatus: '未连接到云',
                        ble_on_off_line: false,
                        //有一种情况就是mqtt断开连接的时候，ble是连接状态的，此时最后一次影子状态是true，
                        //但根据实际情况来说，网关设备断电之后无法上传自己与节点设备的连接情况，因此设置为false才是符合实际的
                    });
                    //wx.showToast({ title: '设备不在线，仅显示设备离线前最后一次数据', icon: 'none', duration: 2000 });
                    //this.getShadow(); // 获取一次影子，作为上次在线数据 
                }
            },
            fail() {
                wx.showToast({ title: '状态查询失败', icon: 'none', duration: 2000 });
            }
        });
    },

    getShadow() {
        //const token = wx.getStorageSync('token');
        const token = this.data.currentToken; // 改为从data中获取
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
                    led: props.led || [],
                    ble: props.ble || [],
                    bleStatus: (props.ble[0] === "true")
                        ? `在线 (${props.ble[1]})`
                        : '离线',
                    ble_on_off_line: (props.ble[0] === "true")
                        ? true
                        : false
                });
            }
        });
    },

    setCommand() {
        //const token = wx.getStorageSync('token');
        const token = this.data.currentToken; // 改为从data中获取
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
                paras: {
                    led_on_off: this.data.led_on_off,
                    ble_on_off: this.data.ble_on_off
                }
            }),
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success(res) {
                if (res.statusCode === 403 && res.data?.error_code === "IOTDA.014016") {
                    wx.showToast({ title: '设备不在线', icon: 'none', duration: 2000 });
                    return;
                };
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


