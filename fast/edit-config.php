<?php
/* edit-config.php | 2026-11 修复版 */
session_start();
$filename = "config.json";

$error = $message = '';
$content = '';

/* ========== 临时：跳过登录（你后面自己加回来） ========== */
$_SESSION['authenticated'] = true;
/* ===================================================== */

/* ---------- 保存 ---------- */
if (isset($_POST['action']) && $_POST['action'] == 'save' && isset($_SESSION['authenticated'])) {
    $src = $_POST['content'] ?? '';

    // 1. PHP 语法检查（仅当内容含 <? 时）
    if (stripos($src, '<?') !== false) {
        $tmp = tempnam(sys_get_temp_dir(), 'phpchk');
        file_put_contents($tmp, $src);
        $ret = 0;
        $out = [];
        exec("php -l " . escapeshellarg($tmp) . " 2>&1", $out, $ret);
        unlink($tmp);

        if ($ret !== 0) {
            $error = implode("\n", $out);
        }
    }

    // 2. 只有语法检查通过才写入
    if (empty($error)) {
        if (file_put_contents($filename, $src) !== false) {
            $message = "文件保存成功";
        } else {
            $error = "保存文件时出错";
        }
    }
}

/* ---------- 读取 ---------- */
if (isset($_SESSION['authenticated']) && file_exists($filename)) {
    $content = file_get_contents($filename);
}
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>简单网页编辑器 · 高亮版</title>

<!-- 样式 -->
<link rel="stylesheet" href="codemirror.css">
<!-- 核心 -->
<script src="codemirror.min.js"></script>
<!-- 语言包 -->
<script src="xml.min.js"></script>
<script src="javascript.min.js"></script>
<script src="css.min.js"></script>
<script src="htmlmixed.min.js"></script>
<script src="php.min.js"></script>

<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#f5f5f5;padding:20px 2%}
.container{width:96%;max-width:none;margin:0 auto;background:#fff;padding:20px;border-radius:5px;box-shadow:0 2px 5px rgba(0,0,0,.1)}
h1{color:#333;text-align:center;margin-bottom:20px}
button{width:100%;padding:10px;margin:5px 0;border:none;border-radius:4px;background:#4CAF50;color:#fff;font-size:16px;cursor:pointer}
button:hover{background:#45a049}
.error{color:red;padding:10px;background:#ffebee;border-radius:4px;margin-bottom:15px;white-space:pre-wrap}
.success{color:green;padding:10px;background:#e8f5e9;border-radius:4px;margin-bottom:15px}
.CodeMirror{height:70vh;font-size:14px;line-height:1.5}
</style>
</head>
<body>
<div class="container">
  <h1>简单网页编辑器</h1>

  <?php if(!empty($message)): ?><div class="success"><?=$message?></div><?php endif; ?>
  <?php if(!empty($error)): ?><div class="error"><?=htmlspecialchars($error)?></div><?php endif; ?>

  <form method="post" id="editForm">
    <input type="hidden" name="action" value="save">
    <textarea id="code" name="content"><?=htmlspecialchars($content)?></textarea>
    <button type="submit">保存更改</button>
  </form>

  <script>
    const editor = CodeMirror.fromTextArea(document.getElementById('code'),{
      lineNumbers: true,
      mode: 'application/x-httpd-php', // ✅ 正确：PHP/HTML 混合
      indentUnit: 2,
      tabSize: 2,
      lineWrapping: true
    });

    document.getElementById('editForm').addEventListener('submit', function(){
      editor.save();
    });
  </script>
</div>
</body>
</html>