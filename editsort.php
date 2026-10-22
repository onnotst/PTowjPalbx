<?php
// 配置文件路径
$configFile = 'config.json';

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

// 处理表单提交
$success = '';
$error = '';
if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $config = readConfig($configFile);
    $action = $_POST['action'] ?? '';

    // 自动重新排序
    if ($action === 'auto') {
        // 1. 按当前配置中的sections顺序重新分配章节排序值
        $sectionOrder = 1;
        foreach ($config['sections'] as &$section) {
            $section['order'] = $sectionOrder++;
            // 2. 对每个链接组内的链接重新排序
            if ($section['type'] === 'links' && isset($section['links'])) {
                $linkOrder = 1;
                foreach ($section['links'] as &$link) {
                    $link['order'] = $linkOrder++;
                }
                unset($link);
            }
        }
        unset($section);
        if (saveConfig($configFile, $config)) {
            $success = '已自动重新排序（章节和链接均按原有顺序从1开始编号）';
        } else {
            $error = '自动排序保存失败，请检查文件权限（建议设置config.json权限为664）';
        }
    }

    // 保存拖拽后的排序
    elseif ($action === 'save_drag') {
        // 解析章节排序数据
        $sectionSortData = json_decode($_POST['section_sort'] ?? '[]', true);
        // 解析链接排序数据
        $linkSortData = json_decode($_POST['link_sort'] ?? '{}', true);

        // 1. 重新排序sections数组（按拖拽后的顺序）
        $sortedSections = [];
        foreach ($sectionSortData as $sectionKey) {
            foreach ($config['sections'] as $section) {
                // 匹配章节唯一标识
                $currentKey = $section['id'] . '_' . $section['type'] . '_' . ($section['title'] ?? mb_substr($section['content'], 0, 20));
                if ($currentKey === $sectionKey) {
                    $sortedSections[] = $section;
                    break;
                }
            }
        }
        // 补充未被拖拽的章节（防止数据丢失）
        foreach ($config['sections'] as $section) {
            $currentKey = $section['id'] . '_' . $section['type'] . '_' . ($section['title'] ?? mb_substr($section['content'], 0, 20));
            if (!in_array($currentKey, $sectionSortData)) {
                $sortedSections[] = $section;
            }
        }
        $config['sections'] = $sortedSections;

        // 2. 为章节分配新的排序值
        $sectionOrder = 1;
        foreach ($config['sections'] as &$section) {
            $section['order'] = $sectionOrder++;
            // 3. 处理链接组内的排序
            if ($section['type'] === 'links' && isset($section['links'])) {
                $sectionTitle = $section['title'] ?? '';
                $sectionId = $section['id'] ?? '';
                $linkKey = $sectionId . '_' . $sectionTitle;
                // 获取该链接组的拖拽排序数据
                $linkIds = $linkSortData[$linkKey] ?? [];
                if (!empty($linkIds)) {
                    $sortedLinks = [];
                    // 按拖拽顺序重新排列链接
                    foreach ($linkIds as $linkName) {
                        foreach ($section['links'] as $link) {
                            if ($link['name'] === $linkName) {
                                $sortedLinks[] = $link;
                                break;
                            }
                        }
                    }
                    // 补充未被拖拽的链接
                    foreach ($section['links'] as $link) {
                        if (!in_array($link['name'], $linkIds)) {
                            $sortedLinks[] = $link;
                        }
                    }
                    $section['links'] = $sortedLinks;
                }
                // 为链接分配新的排序值
                $linkOrder = 1;
                foreach ($section['links'] as &$link) {
                    $link['order'] = $linkOrder++;
                }
                unset($link);
            }
        }
        unset($section);

        // 保存最终配置
        if (saveConfig($configFile, $config)) {
            $success = '拖拽排序已成功保存！所有排序值已重新计算为连续整数';
        } else {
            $error = '拖拽排序保存失败，请检查文件权限';
        }
    }
}

// 读取最新配置用于展示
$config = readConfig($configFile);
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>拖拽排序编辑工具</title>
    <!-- 引入SortableJS -->
    <script src="Sortable.min.js"></script>
    <style>
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
        .btn-group {
            text-align: center;
            margin-bottom: 30px;
            display: flex;
            justify-content: center;
            gap: 20px;
        }
        .btn {
            padding: 10px 25px;
            border: none;
            border-radius: 4px;
            font-size: 16px;
            cursor: pointer;
            transition: background-color 0.3s;
        }
        .btn-auto {
            background-color: #28a745;
            color: white;
        }
        .btn-auto:hover {
            background-color: #218838;
        }
        .btn-save {
            background-color: #007bff;
            color: white;
        }
        .btn-save:hover {
            background-color: #0056b3;
        }
        /* 章节拖拽容器 */
        #sections-container {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }
        .section {
            border: 1px solid #eee;
            border-radius: 6px;
            overflow: hidden;
            cursor: grab;
            transition: box-shadow 0.3s;
        }
        .section:hover {
            box-shadow: 0 2px 8px rgba(0,0,0,0.15);
        }
        .section.grabbing {
            cursor: grabbing;
            opacity: 0.8;
        }
        .section-header {
            background-color: #333;
            color: white;
            padding: 15px 20px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }
        .section-info {
            font-size: 18px;
            font-weight: bold;
        }
        .section-order {
            color: #ccc;
            font-size: 14px;
        }
        /* 链接拖拽容器 */
        .links-container {
            display: flex;
            flex-direction: column;
        }
        .link-item {
            display: flex;
            align-items: center;
            padding: 12px 20px;
            border-bottom: 1px solid #eee;
            cursor: grab;
            transition: background-color 0.3s;
        }
        .link-item:hover {
            background-color: #f9f9f9;
        }
        .link-item.grabbing {
            cursor: grabbing;
            background-color: #f0f0f0;
        }
        .link-item:last-child {
            border-bottom: none;
        }
        .link-name {
            flex: 1;
            font-size: 15px;
            color: #333;
        }
        .link-order {
            color: #999;
            font-size: 14px;
        }
        .quote-content {
            padding: 15px 20px;
            font-style: italic;
            color: #666;
            background-color: #f9f9f9;
            line-height: 1.6;
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
            text-align: center;
            color: #666;
            font-size: 14px;
            margin-top: 30px;
            line-height: 1.6;
        }
        /* 拖拽占位符样式 */
        .sortable-ghost {
            opacity: 0.5;
            background-color: #f0f0f0;
        }
        .sortable-chosen {
            box-shadow: 0 4px 12px rgba(0,0,0,0.2);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>拖拽排序编辑工具</h1>

        <!-- 消息提示 -->
        <?php if ($success): ?>
            <div class="message success"><?php echo $success; ?></div>
        <?php endif; ?>
        <?php if ($error): ?>
            <div class="message error"><?php echo $error; ?></div>
        <?php endif; ?>

        <!-- 操作按钮组 -->
        <div class="btn-group">
            <form method="post" style="display: inline;">
                <input type="hidden" name="action" value="auto">
                <button type="submit" class="btn btn-auto">自动重新排序</button>
            </form>
            <form method="post" id="saveDragForm">
                <input type="hidden" name="action" value="save_drag">
                <input type="hidden" name="section_sort" id="sectionSortInput">
                <input type="hidden" name="link_sort" id="linkSortInput">
                <button type="submit" class="btn btn-save">保存当前排序</button>
            </form>
        </div>

        <!-- 章节拖拽容器 -->
        <div id="sections-container">
            <?php foreach ($config['sections'] as $section): ?>
                <?php
                    // 生成章节唯一标识
                    $sectionKey = $section['id'] . '_' . $section['type'] . '_' . ($section['title'] ?? mb_substr($section['content'], 0, 20));
                    $sectionOrder = isset($section['order']) ? (int)$section['order'] : 1;
                    // 生成链接组唯一标识（用于链接排序）
                    $linkGroupKey = $section['id'] . '_' . ($section['title'] ?? '');
                ?>
                <div class="section" data-section-key="<?php echo htmlspecialchars($sectionKey); ?>">
                    <div class="section-header">
                        <div class="section-info">
                            <?php if ($section['type'] === 'quote'): ?>
                                名言章节 (ID: <?php echo htmlspecialchars($section['id']); ?>)
                            <?php else: ?>
                                链接组：<?php echo htmlspecialchars($section['title'] ?? '未命名'); ?> (ID: <?php echo htmlspecialchars($section['id']); ?>)
                            <?php endif; ?>
                        </div>
                        <div class="section-order">当前排序：<?php echo $sectionOrder; ?></div>
                    </div>

                    <!-- 名言内容展示 -->
                    <?php if ($section['type'] === 'quote'): ?>
                        <div class="quote-content">
                            <?php echo htmlspecialchars($section['content']); ?>
                        </div>
                    <?php endif; ?>

                    <!-- 链接组内拖拽容器 -->
                    <?php if ($section['type'] === 'links' && isset($section['links'])): ?>
                        <div class="links-container" data-link-group="<?php echo htmlspecialchars($linkGroupKey); ?>">
                            <?php foreach ($section['links'] as $link): ?>
                                <?php
                                    $linkOrder = isset($link['order']) ? (int)$link['order'] : 1;
                                ?>
                                <div class="link-item" data-link-name="<?php echo htmlspecialchars($link['name']); ?>">
                                    <div class="link-name"><?php echo htmlspecialchars($link['name']); ?></div>
                                    <div class="link-order">组内排序：<?php echo $linkOrder; ?></div>
                                </div>
                            <?php endforeach; ?>
                        </div>
                    <?php endif; ?>
                </div>
            <?php endforeach; ?>
        </div>

        <div class="hint">
            提示：1. 点击并拖动章节/链接可调整顺序，拖拽时会显示占位符提示；<br>
            2. 调整完成后点击「保存当前排序」，系统会自动计算连续的排序值并保存；<br>
            3. 「自动重新排序」会按现有配置顺序重置排序值，不改变展示顺序。
        </div>
    </div>

    <script>
        // 初始化章节拖拽
        const sectionsContainer = document.getElementById('sections-container');
        const sectionSortable = new Sortable(sectionsContainer, {
            animation: 150,
            ghostClass: 'sortable-ghost',
            chosenClass: 'sortable-chosen',
            onStart: function() {
                // 拖拽开始时添加样式
                document.querySelectorAll('.section').forEach(el => el.classList.add('grabbing'));
            },
            onEnd: function() {
                // 拖拽结束时移除样式
                document.querySelectorAll('.section').forEach(el => el.classList.remove('grabbing'));
                updateSortData(); // 更新排序数据
            }
        });

        // 初始化链接组内拖拽
        const linkContainers = document.querySelectorAll('.links-container');
        linkContainers.forEach(container => {
            new Sortable(container, {
                animation: 150,
                ghostClass: 'sortable-ghost',
                chosenClass: 'sortable-chosen',
                onStart: function() {
                    document.querySelectorAll('.link-item').forEach(el => el.classList.add('grabbing'));
                },
                onEnd: function() {
                    document.querySelectorAll('.link-item').forEach(el => el.classList.remove('grabbing'));
                    updateSortData(); // 更新排序数据
                }
            });
        });

        // 更新排序数据到隐藏域
        function updateSortData() {
            // 收集章节排序数据
            const sectionSort = [];
            document.querySelectorAll('.section').forEach(section => {
                const key = section.getAttribute('data-section-key');
                sectionSort.push(key);
            });
            document.getElementById('sectionSortInput').value = JSON.stringify(sectionSort);

            // 收集链接排序数据
            const linkSort = {};
            document.querySelectorAll('.links-container').forEach(container => {
                const groupKey = container.getAttribute('data-link-group');
                const linkIds = [];
                container.querySelectorAll('.link-item').forEach(link => {
                    const name = link.getAttribute('data-link-name');
                    linkIds.push(name);
                });
                linkSort[groupKey] = linkIds;
            });
            document.getElementById('linkSortInput').value = JSON.stringify(linkSort);
        }

        // 页面加载时先初始化一次排序数据
        window.onload = updateSortData;
    </script>
</body>
</html>