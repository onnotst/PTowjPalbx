/**
 * GM一键全加功能注入脚本
 * 使用方法:将此脚本注入到游戏HTML文件中
 * 
 * 功能说明:
 * - 在GM界面添加"一键全加所有道具"按钮
 * - 点击后自动添加所有道具ID到背包
 * - 包含错误处理和提示信息
 */

// 保存原始的window.__GM值
var __originalGM = window["__GM"];

// 启用GM模式
window["__GM"] = true;

// 等待GMView加载完成后添加按钮
function injectGMButton() {
    // 检查GMView是否存在
    if (typeof GMView !== 'undefined' && GMView.instance && GMView.instance.ui) {
        addOneClickAddAllButton();
    } else {
        // 如果GMView还未加载,等待500ms后重试
        setTimeout(injectGMButton, 500);
    }
}

// 添加一键全加按钮
function addOneClickAddAllButton() {
    try {
        var gmView = GMView.instance;
        var ui = gmView.ui;
        
        // 检查是否已经添加过按钮
        if (ui.m_btn_addAllItems) {
            console.log("一键全加按钮已存在");
            return;
        }
        
        // 创建按钮
        var button = new fgui.GButton();
        button.name = "m_btn_addAllItems";
        button.text = "一键全加";
        button.title = "一键全加";
        
        // 设置按钮样式
        button.setSize(200, 80);
        button.x = 300;
        button.y = 400;
        
        // 设置按钮颜色(红色以突出显示)
        if (button.node && button.node.setColor) {
            button.node.setColor(cc.Color.RED);
        }
        
        // 添加点击事件
        button.onClick(function() {
            addAllItems();
        }, gmView);
        
        // 将按钮添加到UI中
        if (ui.addChild) {
            ui.addChild(button);
        } else if (ui.node && ui.node.addChild) {
            ui.node.addChild(button);
        }
        
        console.log("一键全加按钮添加成功!");
        
    } catch (error) {
        console.error("添加一键全加按钮失败:", error);
    }
}

// 添加所有道具
function addAllItems() {
    try {
        console.log("开始添加所有道具...");
        
        // 获取ItemConfig中的所有道具ID
        var itemIds = [];
        var counts = [];
        
        if (typeof ItemConfig !== 'undefined' && ItemConfig.dataList) {
            // 从ItemConfig.dataList获取所有道具ID
            for (var i = 0; i < ItemConfig.dataList.length; i++) {
                var item = ItemConfig.dataList[i];
                if (item && item.id) {
                    itemIds.push(item.id);
                    counts.push(999); // 每个道具添加999个
                }
            }
        } else {
            // 备选方案:使用GoodsConfig
            console.log("ItemConfig未找到,尝试使用GoodsConfig");
            if (typeof GoodsConfig !== 'undefined' && GoodsConfig.dataList) {
                for (var i = 0; i < GoodsConfig.dataList.length; i++) {
                    var item = GoodsConfig.dataList[i];
                    if (item && item.id) {
                        itemIds.push(item.id);
                        counts.push(999);
                    }
                }
            }
        }
        
        if (itemIds.length === 0) {
            // 如果配置表为空,使用预设的道具ID列表
            console.log("配置表为空,使用预设道具ID");
            itemIds = getDefaultItemIds();
            counts = counts.length > 0 ? counts : itemIds.map(function() { return 999; });
        }
        
        // 使用addItemByItemIds批量添加
        if (typeof YT !== 'undefined' && YT.data && YT.data.addItemByItemIds) {
            YT.data.addItemByItemIds(itemIds, counts);
            console.log("成功添加 " + itemIds.length + " 个道具!");
            
            // 显示提示
            if (typeof gui !== 'undefined' && gui.toast) {
                gui.toast("已添加 " + itemIds.length + " 个道具,每个999个!");
            } else {
                alert("已添加 " + itemIds.length + " 个道具,每个999个!");
            }
        } else if (typeof YT !== 'undefined' && YT.data && YT.data.addItemByItemId) {
            // 逐个添加
            for (var i = 0; i < itemIds.length; i++) {
                YT.data.addItemByItemId(itemIds[i], counts[i] || 999);
            }
            console.log("逐个添加完成,共 " + itemIds.length + " 个道具!");
            
            if (typeof gui !== 'undefined' && gui.toast) {
                gui.toast("已添加 " + itemIds.length + " 个道具,每个999个!");
            } else {
                alert("已添加 " + itemIds.length + " 个道具,每个999个!");
            }
        } else {
            console.error("未找到添加道具的方法!");
            alert("无法添加道具:方法未找到");
        }
        
    } catch (error) {
        console.error("添加道具失败:", error);
        alert("添加道具失败: " + error.message);
    }
}

// 获取默认道具ID列表
function getDefaultItemIds() {
    // 这是一个常见的游戏道具ID范围
    // 根据游戏类型不同,需要调整
    var defaultItems = [];
    
    // 金币(通常ID为1)
    defaultItems.push(1);
    
    // 钻石(通常ID为2)
    defaultItems.push(2);
    
    // 能量/体力(通常ID为3)
    defaultItems.push(3);
    
    // 木材(通常ID为4)
    defaultItems.push(4);
    
    // 经验道具 ID范围 101-200
    for (var i = 101; i <= 200; i++) {
        defaultItems.push(i);
    }
    
    // 武器道具 ID范围 1001-1100
    for (var i = 1001; i <= 1100; i++) {
        defaultItems.push(i);
    }
    
    // 装备道具 ID范围 2001-2100
    for (var i = 2001; i <= 2100; i++) {
        defaultItems.push(i);
    }
    
    // 消耗品 ID范围 3001-3100
    for (var i = 3001; i <= 3100; i++) {
        defaultItems.push(i);
    }
    
    // 皮肤/外观 ID范围 4001-4100
    for (var i = 4001; i <= 4100; i++) {
        defaultItems.push(i);
    }
    
    return defaultItems;
}

// 启动注入
// 等待游戏加载完成后执行
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function() {
        // 延迟2秒等待游戏初始化
        setTimeout(injectGMButton, 2000);
    });
} else {
    // 页面已加载完成,直接执行
    setTimeout(injectGMButton, 2000);
}

console.log("GM一键全加功能已加载!");

// 导出全局函数以便调试
window.__GM_AddAllItems = addAllItems;
window.__GM_InjectGMButton = addOneClickAddAllButton;
