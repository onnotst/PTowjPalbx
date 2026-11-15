<?php
session_start();
$_SESSION['authenticated'] = true;

$filename = 'config.json';
$backupname = 'config.json.bak';
$error = $message = '';

$colors = [
    '' => '默认样式',
    'hight1light' => '浅灰色','hight2light' => '青绿色','hight3light' => '淡紫色',
    'hight4light' => '亮绿色','hight5light' => '深绿色','hight6light' => '浅蓝色',
    'hight7light' => '浅粉色','hight8light' => '橙色','hight9light' => '深蓝色',
    'hight10light' => '金色','hight11light' => '薄荷绿','hight12light' => '蜜桃粉',
    'hight13light' => '薰衣草紫','hight14light' => '柠檬黄','hight15light' => '天蓝色',
    'hight16light' => '珊瑚红','hight17light' => '草绿色','hight18light' => '香槟金',
    'hight19light' => '烟灰色','hight20light' => '玫瑰红','hight21light' => '湖蓝色',
    'hight22light' => '紫罗兰','hight23light' => '橄榄绿','hight24light' => '奶白色',
    'hight25light' => '深紫色','hight26light' => '赤陶色','hight27light' => '海蓝色',
    'hight28light' => '浅绿色','hight29light' => '深红色','hight30light' => '深青色',
];

/* ---------- 读取 ---------- */
$data = [];
$original = []; // 保留原始完整结构
if (file_exists($filename)) {
    $raw = file_get_contents($filename);
    $decoded = json_decode($raw, true);
    if (json_last_error() === JSON_ERROR_NONE && isset($decoded['sections'])) {
        $data = $decoded['sections'];
        $original = $decoded; // 完整原始数据
    } else {
        $error = 'config.json 格式错误';
    }
}

/* ---------- 保存（保留未知字段版） ---------- */
if (isset($_POST['action']) && $_POST['action'] === 'save' && isset($_SESSION['authenticated'])) {

    if (!empty($_POST['blocks_data'])) {
        $submitted = json_decode($_POST['blocks_data'], true);
        if (json_last_error() !== JSON_ERROR_NONE) {
            $error = '提交数据 JSON 解析失败';
            $submitted = [];
        }
    } else {
        $submitted = [];
    }

    // 防清空保护
    if (empty($submitted) && !empty($data)) {
        $error = '提交数据为空，拒绝保存（已阻止清空）';
    } else {
        // 备份
        if (file_exists($filename)) {
            copy($filename, $backupname);
        }

        // 建立原 sections 的索引（按 order 或 id）
        $origSections = [];
        if (!empty($original['sections'])) {
            foreach ($original['sections'] as $s) {
                $key = $s['order'] ?? $s['id'] ?? '';
                if ($key !== '') {
                    $origSections[$key] = $s;
                }
            }
        }

        // 构建新 sections
        $newSections = [];
        $order = 1;
        foreach ($submitted as $block) {
            $newBlock = [];

            if ($block['type'] === 'quote') {
                // 尝试从原数据中找到同 order 的 section，继承未知字段
                $origKey = $order;
                $origBlock = $origSections[$origKey] ?? null;

                if ($origBlock && $origBlock['type'] === 'quote') {
                    // 保留原 block 的所有字段，只覆盖已知的
                    $newBlock = $origBlock;
                }

                $newBlock['id'] = 'j' . str_pad($order, 3, '0', STR_PAD_LEFT);
                $newBlock['type'] = 'quote';
                $newBlock['content'] = $block['content'] ?? '';
                $newBlock['order'] = $order;

            } elseif ($block['type'] === 'links') {
                $origKey = $order;
                $origBlock = $origSections[$origKey] ?? null;

                if ($origBlock && $origBlock['type'] === 'links') {
                    $newBlock = $origBlock;
                }

                $newBlock['id'] = 'j' . str_pad($order, 3, '0', STR_PAD_LEFT);
                $newBlock['type'] = 'links';
                $newBlock['title'] = $block['title'] ?? '';
                $newBlock['order'] = $order;

                // 处理 links 数组
                $links = [];
                if (isset($block['links']) && is_array($block['links'])) {
                    $lorder = 1;
                    // 建原 links 索引
                    $origLinks = [];
                    if (!empty($origBlock['links'])) {
                        foreach ($origBlock['links'] as $ol) {
                            $lk = $ol['order'] ?? $lorder;
                            $origLinks[$lk] = $ol;
                        }
                    }
                    foreach ($block['links'] as $lk) {
                        if (!empty($lk['name']) && !empty($lk['url'])) {
                            $linkEntry = $origLinks[$lorder] ?? [];
                            $linkEntry['name'] = $lk['name'];
                            $linkEntry['url'] = $lk['url'];
                            $linkEntry['color'] = $lk['color'] ?? '';
                            $linkEntry['order'] = $lorder;
                            $links[] = $linkEntry;
                            $lorder++;
                        }
                    }
                }
                $newBlock['links'] = $links;
            }

            $newSections[] = $newBlock;
            $order++;
        }

        // 保留 sections 之外的其他顶层字段（比如你藏备注在根级别）
        $out = $original;
        $out['sections'] = $newSections;

        $json = json_encode($out, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);

        if (file_put_contents($filename, $json) !== false) {
            $message = '保存成功（未知字段已保留，已备份 config.json.bak）';
            $data = $newSections;
        } else {
            $error = '保存失败，请检查文件权限';
        }
    }
}
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>可视化区块编辑器</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#f5f5f5;padding:20px}
.container{max-width:1600px;margin:0 auto;background:#fff;border-radius:8px;padding:20px;box-shadow:0 2px 10px rgba(0,0,0,.1)}
h1{text-align:center;margin-bottom:20px}
.message{color:green;background:#e8f5e9;padding:10px;border-radius:4px;margin-bottom:15px}
.error{color:red;background:#ffebee;padding:10px;border-radius:4px;margin-bottom:15px}
.editor{display:flex;gap:20px;margin-top:20px}
.block-list{flex:1;max-height:70vh;overflow-y:auto;border:1px solid #ddd;border-radius:4px;padding:10px}
.block-edit{flex:1;border:1px solid #ddd;border-radius:4px;padding:15px;max-height:70vh;overflow-y:auto}
.block-item{background:#f9f9f9;border:1px solid #ddd;border-radius:4px;padding:10px;margin-bottom:10px;cursor:move;position:relative}
.block-item:hover{background:#f0f0f0}
.block-item.selected{background:#e3f2fd;border-color:#2196F3}
.block-type{font-size:12px;color:#666;margin-bottom:5px}
.block-title{font-weight:bold;margin-bottom:5px}
.block-actions{position:absolute;right:10px;top:10px}
.block-actions button{padding:2px 8px;margin-left:5px;font-size:12px;border:none;border-radius:3px;cursor:pointer}
.btn-edit{background:#2196F3;color:#fff}
.btn-del{background:#f44336;color:#fff}
.form-group{margin-bottom:15px}
.form-group label{display:block;font-weight:bold;margin-bottom:5px}
.form-group input,.form-group textarea,.form-group select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;font-size:14px}
.form-group textarea{height:100px;resize:vertical}
.link-item{border:1px solid #ddd;border-radius:4px;padding:10px;margin-bottom:10px;background:#fafafa}
.link-item input{margin-bottom:5px}
.btn-add{width:100%;padding:10px;background:#4CAF50;color:#fff;border:none;border-radius:4px;cursor:pointer;margin-bottom:10px}
.btn-add:hover{background:#45a049}
.btn-save{width:100%;padding:12px;background:#2196F3;color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;margin-top:20px}
.btn-save:hover{background:#1976D2}
</style>
</head>
<body>
<div class="container">
<h1>📝 可视化区块编辑器</h1>
<?php if($message): ?><div class="message"><?=$message?></div><?php endif; ?>
<?php if($error): ?><div class="error"><?=$error?></div><?php endif; ?>

<form method="post" id="editorForm">
<input type="hidden" name="action" value="save">
<input type="hidden" name="blocks_data" id="blocksData">

<div class="editor">
<div class="block-list">
    <button type="button" class="btn-add" onclick="addBlock('quote')">+ 添加引言 (Quote)</button>
    <button type="button" class="btn-add" onclick="addBlock('links')">+ 添加链接组 (Links)</button>
    <div id="blocksContainer"></div>
</div>
<div class="block-edit" id="editArea">
    <p style="color:#999;text-align:center;margin-top:50px">← 点击左侧区块进行编辑</p>
</div>
</div>

<button type="button" class="btn-save" onclick="saveAll()">💾 保存全部更改</button>
</form>
</div>

<script>
let blocks = <?=json_encode($data, JSON_UNESCAPED_UNICODE)?>;
let selectedIndex = -1;
const colors = <?=json_encode($colors, JSON_UNESCAPED_UNICODE)?>;

function renderList() {
    const container = document.getElementById('blocksContainer');
    container.innerHTML = '';
    blocks.forEach((b, i) => {
        const div = document.createElement('div');
        div.className = 'block-item' + (i === selectedIndex ? ' selected' : '');
        div.draggable = true;
        div.ondragstart = (e) => { e.dataTransfer.setData('text/plain', i); div.style.opacity='0.5'; };
        div.ondragend = () => { div.style.opacity='1'; };
        div.ondragover = (e) => { e.preventDefault(); };
        div.ondrop = (e) => {
            e.preventDefault();
            const from = parseInt(e.dataTransfer.getData('text/plain'));
            const to = i;
            if (from !== to) {
                const item = blocks.splice(from, 1)[0];
                blocks.splice(to, 0, item);
                selectedIndex = to;
                renderList();
            }
        };
        div.onclick = () => { selectedIndex = i; renderList(); renderEdit(); };

        let preview = '';
        if (b.type === 'quote') {
            let c = b.content.replace(/<[^>]*>/g, '');
            preview = c.substring(0, 50) + (c.length > 50 ? '...' : '');
        } else {
            preview = b.title + ' (' + (b.links ? b.links.length : 0) + ' 个链接)';
        }

        div.innerHTML = `<div class="block-type">[${b.type}]</div><div class="block-title">${preview}</div><div class="block-actions"><button class="btn-edit" onclick="event.stopPropagation();selectedIndex=${i};renderEdit()">编辑</button><button class="btn-del" onclick="event.stopPropagation();deleteBlock(${i})">删除</button></div>`;
        container.appendChild(div);
    });
}

function addBlock(type) {
    if (type === 'quote') {
        blocks.push({id:'', type:'quote', content:'', order:0});
    } else {
        blocks.push({id:'', type:'links', title:'', links:[], order:0});
    }
    selectedIndex = blocks.length - 1;
    renderList();
    renderEdit();
}

function deleteBlock(i) {
    if (confirm('确定删除？')) {
        blocks.splice(i, 1);
        if (selectedIndex >= blocks.length) selectedIndex = blocks.length - 1;
        renderList();
        renderEdit();
    }
}

function renderEdit() {
    const area = document.getElementById('editArea');
    if (selectedIndex < 0 || selectedIndex >= blocks.length) {
        area.innerHTML = '<p style="color:#999;text-align:center;margin-top:50px">← 点击左侧区块</p>';
        return;
    }
    const b = blocks[selectedIndex];
    if (b.type === 'quote') {
        area.innerHTML = `<h3>编辑引言</h3><div class="form-group"><label>内容（支持 HTML）</label><textarea id="editContent">${b.content.replace(/"/g,'&quot;')}</textarea></div><button type="button" class="btn-add" onclick="updateQuote()">✓ 更新</button>`;
    } else {
        let html = `<h3>编辑链接组</h3><div class="form-group"><label>组标题</label><input type="text" id="editTitle" value="${(b.title||'').replace(/"/g,'&quot;')}"></div><div id="linksContainer">`;
        if (b.links && b.links.length > 0) {
            b.links.forEach((l, li) => {
                html += `<div class="link-item" data-link-index="${li}" draggable="true" ondragstart="linkDragStart(event,${li})" ondragover="linkDragOver(event)" ondrop="linkDrop(event,${li})" ondragend="linkDragEnd(event)">
                    <div style="display:flex;gap:5px;margin-bottom:5px">
                        <input type="text" placeholder="名称" value="${(l.name||'').replace(/"/g,'&quot;')}" onchange="updateLink(${li},'name',this.value)" style="flex:1">
                        <button type="button" onclick="moveLinkUp(${li})" style="background:#ff9800;color:#fff;border:none;padding:5px 8px;border-radius:3px;cursor:pointer;font-size:12px">↑</button>
                        <button type="button" onclick="moveLinkDown(${li})" style="background:#795548;color:#fff;border:none;padding:5px 8px;border-radius:3px;cursor:pointer;font-size:12px">↓</button>
                        <button type="button" onclick="removeLink(${li})" style="background:#f44336;color:#fff;border:none;padding:5px 8px;border-radius:3px;cursor:pointer;font-size:12px">×</button>
                    </div>
                    <input type="text" placeholder="URL" value="${(l.url||'').replace(/"/g,'&quot;')}" onchange="updateLink(${li},'url',this.value)" style="width:100%;margin-bottom:5px;box-sizing:border-box">
                    <select onchange="updateLink(${li},'color',this.value)" style="width:100%">
                        ${Object.entries(colors).map(([k,v]) => `<option value="${k}" ${(l.color||'')===k?'selected':''}>${v}</option>`).join('')}
                    </select>
                </div>`;
            });
        }
        html += `</div><button type="button" class="btn-add" onclick="addLink()">+ 添加链接</button><button type="button" class="btn-add" onclick="updateLinksBlock()">✓ 更新链接组</button>`;
        area.innerHTML = html;
    }
}

/* ---------- 链接拖拽 ---------- */
let linkDragSrc = null;
function linkDragStart(e, index) {
    linkDragSrc = index;
    e.target.style.opacity = '0.5';
}
function linkDragOver(e) {
    e.preventDefault();
}
function linkDrop(e, index) {
    e.preventDefault();
    if (linkDragSrc !== null && linkDragSrc !== index) {
        const item = blocks[selectedIndex].links.splice(linkDragSrc, 1)[0];
        blocks[selectedIndex].links.splice(index, 0, item);
        renderEdit();
    }
}
function linkDragEnd(e) {
    e.target.style.opacity = '1';
}

/* ---------- 链接上移/下移 ---------- */
function moveLinkUp(i) {
    if (i > 0) {
        const item = blocks[selectedIndex].links.splice(i, 1)[0];
        blocks[selectedIndex].links.splice(i - 1, 0, item);
        renderEdit();
    }
}
function moveLinkDown(i) {
    if (i < blocks[selectedIndex].links.length - 1) {
        const item = blocks[selectedIndex].links.splice(i, 1)[0];
        blocks[selectedIndex].links.splice(i + 1, 0, item);
        renderEdit();
    }
}

function updateQuote() {
    blocks[selectedIndex].content = document.getElementById('editContent').value;
    renderList();
    alert('已更新');
}

function addLink() {
    if (!blocks[selectedIndex].links) blocks[selectedIndex].links = [];
    blocks[selectedIndex].links.push({name:'', url:'', color:'', order:blocks[selectedIndex].links.length+1});
    renderEdit();
}

function removeLink(i) {
    blocks[selectedIndex].links.splice(i, 1);
    renderEdit();
}

function updateLink(i, field, val) {
    blocks[selectedIndex].links[i][field] = val;
}

function updateLinksBlock() {
    blocks[selectedIndex].title = document.getElementById('editTitle').value;
    renderList();
    alert('已更新');
}

function saveAll() {
    document.getElementById('blocksData').value = JSON.stringify(blocks);
    document.getElementById('editorForm').submit();
}

renderList();
selectedIndex = -1;
document.getElementById('editArea').innerHTML = '<p style="color:#999;text-align:center;margin-top:50px">← 点击左侧区块进行编辑</p>';
</script>
</body>
</html>