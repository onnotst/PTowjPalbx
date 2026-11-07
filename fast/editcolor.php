<?php
// 配置文件路径
$configFile = 'config.json';
// 定义颜色样式映射（原有5种+新增5种）
$colorStyles = [
    '' => '默认样式',
    'hight1light' => '浅灰色 (#e9e9e9)',
    'hight2light' => '青绿色 (#94e2ce)',
    'hight3light' => '淡紫色 (#bfb4f9)',
    'hight4light' => '亮绿色 (#9bff44)',
    'hight5light' => '深绿色 (#7bcc36)',
    'hight6light' => '浅蓝色 (#a0d2eb)',
    'hight7light' => '浅粉色 (#ffc2d1)',
    'hight8light' => '橙色 (#ffb347)',
    'hight9light' => '深蓝色 (#4169e1)',
    'hight10light' => '金色 (#ffd700)'
];

// 处理表单提交
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['color_data'])) {
    // 读取原始配置
    $configData = json_decode(file_get_contents($configFile), true);
    if (json_last_error() !== JSON_ERROR_NONE) {
        $error = '配置文件解析失败：' . json_last_error_msg();
    } else {
        // 解析提交的颜色数据
        $colorData = $_POST['color_data'];
        // 遍历配置更新颜色
        foreach ($configData['sections'] as &$section) {
            if ($section['type'] === 'links' && isset($section['links'])) {
                foreach ($section['links'] as &$link) {
                    $linkId = $section['id'] . '_' . $section['title'] . '_' . $link['name'];
                    if (isset($colorData[$linkId])) {
                        $link['color'] = $colorData[$linkId];
                    }
                }
                unset($link); // 释放引用
            }
        }
        unset($section); // 释放引用

        // 保存修改后的配置
        $jsonOptions = JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES;
        if (file_put_contents($configFile, json_encode($configData, $jsonOptions))) {
            $success = '颜色配置已成功保存！';
        } else {
            $error = '配置文件保存失败，请检查文件权限！';
        }
    }
}

// 读取配置用于展示
$configData = json_decode(file_get_contents($configFile), true);
if (json_last_error() !== JSON_ERROR_NONE) {
    die('配置文件读取失败：' . json_last_error_msg());
}
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>链接颜色编辑工具</title>
    <style>
        /* 引入所有高亮样式 */
        .hight1light { background-color: #e9e9e9; }
        .hight2light { background-color: #94e2ce; }
        .hight3light { background-color: #bfb4f9; }
        .hight4light { background-color: #9bff44; }
        .hight5light { background-color: #7bcc36; }
        .hight6light { background-color: #a0d2eb; }
        .hight7light { background-color: #ffc2d1; }
        .hight8light { background-color: #ffb347; }
        .hight9light { background-color: #4169e1; color: white; }
        .hight10light { background-color: #ffd700; }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: "Microsoft YaHei", sans-serif;
        }
        body {
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            padding: 30px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
        }
        .section {
            margin-bottom: 40px;
            border: 1px solid #eee;
            border-radius: 6px;
            overflow: hidden;
        }
        .section-title {
            background-color: #333;
            color: white;
            padding: 15px 20px;
            font-size: 18px;
            font-weight: bold;
        }
        .link-item {
            display: flex;
            align-items: center;
            padding: 15px 20px;
            border-bottom: 1px solid #eee;
            transition: background-color 0.2s;
        }
        .link-item:hover {
            background-color: #f9f9f9;
        }
        .link-item:last-child {
            border-bottom: none;
        }
        .link-name {
            flex: 1;
            font-size: 16px;
            color: #333;
        }
        .color-preview {
            width: 100px;
            height: 36px;
            border: 1px solid #ddd;
            border-radius: 4px;
            margin: 0 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 13px;
            color: #666;
            transition: all 0.2s ease;
            font-weight: 500;
        }
        /* 默认样式的预览背景 */
        .color-preview.default {
            background-color: #ffffff;
            border: 1px dashed #999;
        }
        .color-select {
            padding: 10px 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
            color: #333;
            cursor: pointer;
            min-width: 200px;
            transition: border-color 0.2s;
        }
        .color-select:focus {
            outline: none;
            border-color: #007bff;
            box-shadow: 0 0 0 2px rgba(0,123,255,0.25);
        }
        .submit-btn {
            display: block;
            width: 220px;
            height: 50px;
            line-height: 50px;
            text-align: center;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            cursor: pointer;
            margin: 0 auto;
            transition: background-color 0.3s;
        }
        .submit-btn:hover {
            background-color: #0056b3;
        }
        .submit-btn:active {
            transform: scale(0.98);
        }
        .message {
            text-align: center;
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 4px;
            font-size: 16px;
        }
        .success {
            background-color: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .error {
            background-color: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .color-desc {
            font-size: 12px;
            color: #999;
            text-align: center;
            margin-top: 20px;
            line-height: 1.6;
        }
        .color-swatch {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 2px;
            margin-right: 4px;
            vertical-align: middle;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>链接颜色编辑工具</h1>
        
        <?php if (isset($success)): ?>
            <div class="message success"><?php echo $success; ?></div>
        <?php endif; ?>
        <?php if (isset($error)): ?>
            <div class="message error"><?php echo $error; ?></div>
        <?php endif; ?>

        <form method="post">
            <?php foreach ($configData['sections'] as $section): ?>
                <?php if ($section['type'] === 'links' && isset($section['links'])): ?>
                    <div class="section">
                        <div class="section-title">
                            <?php echo htmlspecialchars($section['title']); ?> (排序：<?php echo $section['order']; ?>)
                        </div>
                        <?php foreach ($section['links'] as $link): ?>
                            <?php 
                                $linkId = $section['id'] . '_' . $section['title'] . '_' . $link['name'];
                                $currentColor = $link['color'] ?? '';
                                // 获取当前颜色的描述
                                $currentDesc = $colorStyles[$currentColor] ?? '默认样式';
                            ?>
                            <div class="link-item">
                                <div class="link-name"><?php echo htmlspecialchars($link['name']); ?></div>
                                <div class="color-preview <?php echo $currentColor ?: 'default'; ?>" data-preview-for="<?php echo htmlspecialchars($linkId); ?>">
                                    <?php echo $currentColor ?: '默认'; ?>
                                </div>
                                <select name="color_data[<?php echo htmlspecialchars($linkId); ?>]" class="color-select" data-select-for="<?php echo htmlspecialchars($linkId); ?>">
                                    <?php foreach ($colorStyles as $style => $desc): ?>
                                        <option value="<?php echo htmlspecialchars($style); ?>" 
                                            <?php echo $style === $currentColor ? 'selected' : ''; ?>>
                                            <?php if ($style): ?>
                                                <span class="color-swatch <?php echo htmlspecialchars($style); ?>"></span>
                                            <?php endif; ?>
                                            <?php echo htmlspecialchars($desc); ?>
                                        </option>
                                    <?php endforeach; ?>
                                </select>
                            </div>
                        <?php endforeach; ?>
                    </div>
                <?php endif; ?>
            <?php endforeach; ?>

            <button type="submit" class="submit-btn">保存颜色配置</button>
        </form>

        <div class="color-desc">
            颜色样式说明：<br>
            hight1light(浅灰)、hight2light(青绿)、hight3light(淡紫)、hight4light(亮绿)、hight5light(深绿)、<br>
            hight6light(浅蓝)、hight7light(浅粉)、hight8light(橙色)、hight9light(深蓝)、hight10light(金色)
        </div>
    </div>

    <script>
        // 页面加载完成后初始化
        document.addEventListener('DOMContentLoaded', function() {
            // 获取所有颜色选择下拉框
            const colorSelects = document.querySelectorAll('.color-select');
            
            // 为每个下拉框绑定change事件
            colorSelects.forEach(select => {
                select.addEventListener('change', function() {
                    // 获取当前选择的颜色样式值
                    const selectedValue = this.value;
                    // 获取对应的预览区域ID
                    const linkId = this.getAttribute('data-select-for');
                    const previewEl = document.querySelector(`.color-preview[data-preview-for="${linkId}"]`);
                    
                    if (previewEl) {
                        // 移除所有颜色样式类
                        previewEl.classList.remove(...Object.keys(<?php echo json_encode($colorStyles); ?>).filter(style => style));
                        previewEl.classList.remove('default');
                        
                        // 添加选中的样式类或默认样式
                        if (selectedValue) {
                            previewEl.classList.add(selectedValue);
                            previewEl.textContent = selectedValue;
                        } else {
                            previewEl.classList.add('default');
                            previewEl.textContent = '默认';
                        }
                    }
                });

                // 初始化下拉框的option样式（添加颜色小方块）
                const options = select.querySelectorAll('option');
                options.forEach(option => {
                    const html = option.innerHTML;
                    // 移除已有的color-swatch，避免重复
                    option.innerHTML = html.replace(/<span class="color-swatch.*?><\/span>/g, '');
                    // 为非默认选项添加颜色小方块
                    if (option.value) {
                        option.innerHTML = `<span class="color-swatch ${option.value}"></span>${option.innerHTML}`;
                    }
                });
            });
        });
    </script>
</body>
</html>