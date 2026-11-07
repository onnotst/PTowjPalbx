<?php
// 配置文件路径
$configFile = 'config.json';
// 定义颜色样式映射（与editcolor.php保持一致）
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

// 读取配置文件的通用函数
function readConfig($file) {
    if (!file_exists($file)) {
        die("错误：配置文件 $file 不存在");
    }
    $content = file_get_contents($file);
    $config = json_decode($content, true);
    if (json_last_error() !== JSON_ERROR_NONE) {
        die("错误：配置文件解析失败 - " . json_last_error_msg());
    }
    return $config;
}

// 保存配置文件的通用函数
function saveConfig($file, $config) {
    $jsonOptions = JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES;
    $content = json_encode($config, $jsonOptions);
    if (file_put_contents($file, $content) === false) {
        return false;
    }
    return true;
}

// 验证URL格式
function validateUrl($url) {
    $pattern = '/^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$/';
    return preg_match($pattern, $url) || empty($url); // 允许空（后端会再次检查）
}

// 处理表单提交
$success = '';
$error = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $config = readConfig($configFile);
    // 获取表单数据
    $targetGroup = $_POST['target_group'] ?? '';
    $linkName = trim($_POST['link_name'] ?? '');
    $linkUrl = trim($_POST['link_url'] ?? '');
    $linkColor = $_POST['link_color'] ?? '';

    // 表单验证
    if (empty($targetGroup)) {
        $error = '请选择要添加的目标链接分组';
    } elseif (empty($linkName)) {
        $error = '链接名称不能为空';
    } elseif (empty($linkUrl)) {
        $error = '链接URL不能为空';
    } elseif (!validateUrl($linkUrl)) {
        $error = 'URL格式不合法，请输入以http/https开头的有效地址';
    } elseif (!array_key_exists($linkColor, $colorStyles)) {
        $error = '选择的颜色样式不合法';
    } else {
        // 查找目标分组并新增链接
        $groupFound = false;
        foreach ($config['sections'] as &$section) {
            if ($section['type'] === 'links' && $section['title'] === $targetGroup) {
                $groupFound = true;
                // 计算新链接的排序值（最大排序值+1）
                $maxOrder = 0;
                foreach ($section['links'] as $link) {
                    $currentOrder = isset($link['order']) ? (int)$link['order'] : 0;
                    if ($currentOrder > $maxOrder) {
                        $maxOrder = $currentOrder;
                    }
                }
                $newOrder = $maxOrder + 1;

                // 构造新链接数据（移除_comment字段）
                $newLink = [
                    'name' => $linkName,
                    'url' => $linkUrl,
                    'color' => $linkColor,
                    'order' => $newOrder
                ];

                // 添加到分组的links数组末尾
                $section['links'][] = $newLink;
                break;
            }
        }
        unset($section);

        if (!$groupFound) {
            $error = '目标链接分组不存在';
        } else {
            // 保存配置
            if (saveConfig($configFile, $config)) {
                $success = "链接「{$linkName}」已成功添加到「{$targetGroup}」分组（排序值：{$newOrder}）";
                // 清空表单数据（优化体验）
                $_POST = [];
            } else {
                $error = '链接添加失败，请检查config.json文件权限（建议设置为664）';
            }
        }
    }
} // 修复：补充缺失的闭合花括号

// 读取配置，获取所有链接分组
$config = readConfig($configFile);
$linkGroups = [];
foreach ($config['sections'] as $section) {
    if ($section['type'] === 'links' && !empty($section['title'])) {
        $linkGroups[] = $section['title'];
    }
}
if (empty($linkGroups)) {
    die("错误：配置文件中未找到有效的链接分组");
}
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>新增链接工具</title>
    <style>
        /* 颜色样式与editcolor.php保持一致 */
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
            max-width: 800px;
            margin: 0 auto;
            background: white;
            padding: 40px;
            border-radius: 8px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 30px;
        }
        .form-group {
            margin-bottom: 25px;
        }
        .form-label {
            display: block;
            font-size: 16px;
            color: #333;
            margin-bottom: 8px;
            font-weight: 500;
        }
        .form-control {
            width: 100%;
            padding: 12px 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 15px;
            color: #333;
            transition: border-color 0.2s;
        }
        .form-control:focus {
            outline: none;
            border-color: #007bff;
            box-shadow: 0 0 0 2px rgba(0,123,255,0.25);
        }
        .form-select {
            width: 100%;
            padding: 12px 15px;
            border: 1px solid #ddd;
            border-radius: 4px;
            font-size: 15px;
            color: #333;
            cursor: pointer;
        }
        .submit-btn {
            display: block;
            width: 200px;
            height: 48px;
            line-height: 48px;
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
        .submit-btn:disabled {
            background-color: #6c757d;
            cursor: not-allowed;
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
        .hint {
            font-size: 14px;
            color: #666;
            margin-top: 5px;
            line-height: 1.5;
        }
        .color-swatch {
            display: inline-block;
            width: 12px;
            height: 12px;
            border-radius: 2px;
            margin-right: 6px;
            vertical-align: middle;
        }
        .preview-tip {
            text-align: center;
            margin-top: 30px;
            font-size: 14px;
            color: #999;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>新增链接工具</h1>

        <!-- 消息提示 -->
        <?php if ($success): ?>
            <div class="message success"><?php echo $success; ?></div>
        <?php endif; ?>
        <?php if ($error): ?>
            <div class="message error"><?php echo $error; ?></div>
        <?php endif; ?>

        <!-- 新增链接表单 -->
        <form id="addLinkForm" method="post">
            <!-- 目标分组选择 -->
            <div class="form-group">
                <label class="form-label" for="target_group">目标链接分组 *</label>
                <select name="target_group" id="target_group" class="form-select" required>
                    <option value="">请选择分组</option>
                    <?php foreach ($linkGroups as $group): ?>
                        <option value="<?php echo htmlspecialchars($group); ?>" <?php echo (isset($_POST['target_group']) && $_POST['target_group'] === $group) ? 'selected' : ''; ?>>
                            <?php echo htmlspecialchars($group); ?>
                        </option>
                    <?php endforeach; ?>
                </select>
            </div>

            <!-- 链接名称 -->
            <div class="form-group">
                <label class="form-label" for="link_name">链接名称 *</label>
                <input type="text" name="link_name" id="link_name" class="form-control" 
                       value="<?php echo htmlspecialchars($_POST['link_name'] ?? ''); ?>" 
                       placeholder="例如：百度搜索" required>
            </div>

            <!-- 链接URL -->
            <div class="form-group">
                <label class="form-label" for="link_url">链接URL *</label>
                <input type="url" name="link_url" id="link_url" class="form-control" 
                       value="<?php echo htmlspecialchars($_POST['link_url'] ?? ''); ?>" 
                       placeholder="例如：https://www.baidu.com" required>
                <div class="hint">请输入以http/https开头的有效URL地址</div>
            </div>

            <!-- 颜色样式选择 -->
            <div class="form-group">
                <label class="form-label" for="link_color">颜色样式</label>
                <select name="link_color" id="link_color" class="form-select">
                    <?php foreach ($colorStyles as $style => $desc): ?>
                        <option value="<?php echo htmlspecialchars($style); ?>" <?php echo (isset($_POST['link_color']) && $_POST['link_color'] === $style) ? 'selected' : ''; ?>>
                            <?php if ($style): ?>
                                <span class="color-swatch <?php echo htmlspecialchars($style); ?>"></span>
                            <?php endif; ?>
                            <?php echo htmlspecialchars($desc); ?>
                        </option>
                    <?php endforeach; ?>
                </select>
                <div class="hint">选择链接的高亮样式，默认为无样式</div>
            </div>

            <button type="submit" class="submit-btn">添加链接</button>
        </form>

        <div class="preview-tip">
            提示：新增的链接会默认排在对应分组的最后一位，可通过editsort.php调整排序
        </div>
    </div>

    <script>
        // 表单验证与提交控制
        document.addEventListener('DOMContentLoaded', function() {
            const form = document.getElementById('addLinkForm');
            const submitBtn = form.querySelector('.submit-btn');
            const requiredFields = form.querySelectorAll('[required]');

            // 实时检查必填项
            function checkRequired() {
                let allFilled = true;
                requiredFields.forEach(field => {
                    if (!field.value.trim()) {
                        allFilled = false;
                    }
                });
                submitBtn.disabled = !allFilled;
            }

            // 为必填项绑定事件
            requiredFields.forEach(field => {
                field.addEventListener('input', checkRequired);
                field.addEventListener('change', checkRequired);
            });

            // 初始化检查
            checkRequired();

            // URL格式前端验证
            const urlInput = document.getElementById('link_url');
            urlInput.addEventListener('blur', function() {
                const url = this.value.trim();
                const pattern = /^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$/;
                if (url && !pattern.test(url)) {
                    alert('URL格式不合法，请输入以http/https开头的有效地址');
                    this.focus();
                }
            });

            // 初始化下拉框的颜色小方块
            const colorSelect = document.getElementById('link_color');
            const options = colorSelect.querySelectorAll('option');
            options.forEach(option => {
                const html = option.innerHTML;
                option.innerHTML = html.replace(/<span class="color-swatch.*?><\/span>/g, '');
                if (option.value) {
                    option.innerHTML = `<span class="color-swatch ${option.value}"></span>${option.innerHTML}`;
                }
            });
        });
    </script>
</body>
</html>