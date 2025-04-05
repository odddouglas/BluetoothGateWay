Page({
    data: {
        //设备属性
        temperature: 0.0,
        humidity: 0.0,
        led_state: true,
        led_on_off: true,
        //POST https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens
        tokenUrl: 'https://iam.cn-north-4.myhuaweicloud.com/v3/auth/tokens',
        //GET https://{endpoint}/v5/iot/{project_id}/devices/{device_id}/shadow
        shadowUrl: 'https://ed6cc26730.st1.iotda-app.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/shadow',
        //POST https://{endpoint}/v5/iot/{project_id}/devices/{device_id}/commands
        commandUrl: 'https://ed6cc26730.st1.iotda-app.cn-north-4.myhuaweicloud.com/v5/iot/5631b5e6a3a34c86bc2e1cbd09ae9fc9/devices/67ed58015367f573f77ef961_esp32/commands',
        projectId: 'cn-north-4',  // 项目ID
        deviceId: '67ed58015367f573f77ef961_esp32',  // 设备ID
        serviceId: 'gateway_data',  // 服务ID
        commandName: 'ctrl',  // 命令名称

        // 认证信息
        authDomain: "odddouglas",  // 主用户名
        authUser: "iota",         // IAM用户名
        authPassword: "qgddgls1128", // IAM密码
    },

    // 页面生命周期函数
    onLoad() {
        console.log("页面 onLoad");
    },
    onShow() {
        console.log("页面 onShow");
    },

    // 开关变化
    handleChange(e) {
        this.setData({
            led_on_off: e.detail.value,
        });
        this.setCommand();
    },

    // 按钮1：获取 token
    handleButton1(e) {
        this.getToken();
        console.log(e);
    },

    // 按钮2：获取设备影子
    handleButton2(e) {
        this.getShadow();
        console.log(e);
    },


    // 获取 token
    getToken() {
        console.log("开始获取 token...");
        wx.request({
            url: this.data.tokenUrl,
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
            method: 'POST',
            header: { 'Content-Type': 'application/json' },
            success(res) {
                console.log("获取token成功");
                const token = res.header['X-Subject-Token'];
                wx.setStorageSync('token', token);
                console.log("token:", token);
            },
            fail() {
                console.log("获取token失败");
            },
            complete() {
                console.log("获取token完成");
            }
        });
    },

    // 获取设备影子
    getShadow() {
        let that = this;  //异步
        console.log("开始获取影子");
        const token = wx.getStorageSync('token');
        console.log("当前 token:", token);
        wx.request({
            url: this.data.shadowUrl,
            method: 'GET',
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success(res) {
                console.log("获取影子成功");
                const shadow = res.data.shadow[0]?.reported?.properties || {};
                console.log("影子数据：", shadow);
                that.setData({
                    temperature: shadow.temperature || 0,
                    humidity: shadow.humidity || 0,
                    led_state: shadow.led || false
                });
            },
            fail() {
                console.log("获取影子失败");
            },
            complete() {
                console.log("获取影子完成");
            }
        });
    },

    // 下发命令
    setCommand() {
        console.log("开始下发命令");
        const token = wx.getStorageSync('token');
        const cmd = this.data.led_on_off;
        wx.request({
            url: this.data.commandUrl,
            method: 'POST',
            data: JSON.stringify({
                service_id: this.data.serviceId,
                command_name: this.data.commandName,
                paras: { led_on_off: cmd }
            }),
            header: {
                'content-type': 'application/json',
                'X-Auth-Token': token
            },
            success(res) {
                console.log("下发命令成功");
                console.log(res);
            },
            fail() {
                console.log("命令下发失败，请先获取token");
            },
            complete() {
                console.log("命令下发完成");
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


