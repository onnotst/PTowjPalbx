<?php
/*  editpro.php  |  2025-06  */
session_start();
$valid_username = "onno";
$valid_password = "Howtomakemoney1.";
$filename = "config.json";

$error = $message = $content = '';
/* ---------- 登录 ---------- */
if (isset($_POST['action']) && $_POST['action'] == 'login') {
    if ($_POST['username'] === $valid_username && $_POST['password'] === $valid_password) {
        $_SESSION['authenticated'] = true;
    } else {
        $error = "用户名或密码错误";
    }
}
/* ---------- 登出 ---------- */
if (isset($_GET['action']) && $_GET['action'] == 'logout') {
    session_destroy();
    header("Location: editpro.php");
    exit;
}
/* ---------- 保存 ---------- */
if (isset($_POST['action']) && $_POST['action'] == 'save' && isset($_SESSION['authenticated'])) {
    $src = $_POST['content'] ?? '';
    // 1. 语法检查（仅当内容含 <?php 时）
    if (stripos($src, '<?') !== false) {
        $tmp = tempnam(sys_get_temp_dir(), 'phpchk');
        file_put_contents($tmp, $src);
        $ret = 0;
        $out = [];
        exec("php -l " . escapeshellarg($tmp) . " 2>&1", $out, $ret);
        unlink($tmp);
        if ($ret !== 0) {
            $error = implode("\n", $out);
            goto SHOW;          // 拦截保存
        }
    }
    // 2. 写入
    if (file_put_contents($filename, $src) !== false) {
        $message = "文件保存成功";
    } else {
        $error = "保存文件时出错";
    }
}
/* ---------- 读取 ---------- */
if (isset($_SESSION['authenticated']) && file_exists($filename)) {
    $content = htmlspecialchars(file_get_contents($filename));
}
SHOW:
?>
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>简单网页编辑器 · 高亮版</title>
<!-- 样式 -->
<!--<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/lib/codemirror.css">-->
<!-- 核心 -->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/lib/codemirror.min.js"></script>-->
<!-- 语言包（HTML+PHP 混合） -->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/mode/xml/xml.min.js"></script>-->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/mode/javascript/javascript.min.js"></script>-->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/mode/css/css.min.js"></script>-->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/mode/htmlmixed/htmlmixed.min.js"></script>-->
<!--<script src="https://cdn.jsdelivr.net/npm/codemirror@5.65.16/mode/php/php.min.js"></script>-->

<!-- 样式 -->
<link rel="stylesheet" href="codemirror.css">
<!-- 核心 -->
<script src="codemirror.min.js"></script>
<!-- 语言包（HTML+PHP 混合） -->
<script src="xml.min.js"></script>
<script src="javascript.min.js"></script>
<script src="css.min.js"></script>
<script src="htmlmixed.min.js"></script>
<script src="php.min.js"></script>

<style>
/* 沿用你原来的样式，仅把 textarea 换成 CodeMirror 占满 */
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Arial,sans-serif;background:#f5f5f5;padding:20px 2%}
.container{width:96%;max-width:none;margin:0 auto;background:#fff;padding:20px;border-radius:5px;box-shadow:0 2px 5px rgba(0,0,0,.1)}
h1{color:#333;text-align:center;margin-bottom:20px}
.logout{text-align:right;margin-bottom:10px}
.logout a{color:#666;text-decoration:none}
.logout a:hover{text-decoration:underline}
button{width:100%;padding:10px;margin:5px 0;border:none;border-radius:4px;background:#4CAF50;color:#fff;font-size:16px;cursor:pointer}
button:hover{background:#45a049}
.error{color:red;padding:10px;background:#ffebee;border-radius:4px;margin-bottom:15px}
.success{color:green;padding:10px;background:#e8f5e9;border-radius:4px;margin-bottom:15px}
.CodeMirror{height:70vh;font-size:14px;line-height:1.5}
.info-text{text-align:center;font-size:14px;color:#666;margin-top:10px}
/* 登录框保持原样 */
input[type=text],input[type=password]{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:4px}
.xxxx-div {
    display: flex;
    justify-content: center;
    align-items: center;
    padding: 32px 0;
    width: 100%;
}
.xxxx-a {
    display: inline-block;
    background-color: #10b981;
    color: white;
    font-weight: 600;
    padding: 12px 32px;
    border-radius: 8px;
    text-decoration: none;
    font-size: 1.125rem;
    box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
    transition: all 0.3s ease-in-out;
    text-align: center;
}
.xxxx-a:hover {
    background-color: #059669;
    transform: translateY(-2px);
    box-shadow: 0 6px 8px rgba(0, 0, 0, 0.15);
}
</style>
</head>
<body>
<div class="container">
  <h1>简单网页编辑器</h1>

<?php if(isset($_SESSION['authenticated'])): ?>
  <div class="logout"><a href="?action=logout">登出</a></div>

  <?php if(!empty($message)): ?><div class="success"><?=$message?></div><?php endif; ?>
  <?php if(!empty($error)): ?><div class="error"><?=nl2br($error)?></div><?php endif; ?>

  <form method="post" id="editForm">
    <input type="hidden" name="action" value="save">
    <textarea id="code" name="content"><?=isset($content)?$content:''?></textarea>
    <button type="submit">保存更改</button>
  </form>

  <script>
    /* 初始化 CodeMirror：支持 HTML+PHP 混合 */
    const editor = CodeMirror.fromTextArea(document.getElementById('code'),{
      lineNumbers: true,
      mode: 'javascript',   // 自动识别 HTML 内嵌 PHP/JS/CSS
      indentUnit: 2,
      tabSize: 2,
      lineWrapping: true
    });

    /* 保存前把 CodeMirror 值写回 textarea */
    document.getElementById('editForm').addEventListener('submit',()=>{
      editor.save();
    });
  </script>

<?php else: ?>
  <?php if(!empty($error)): ?><div class="error"><?=$error?></div><?php endif; ?>
  <form method="post" class="login-form">
    <input type="hidden" name="action" value="login">
    <input type="text" name="username" placeholder="用户名" required>
    <input type="password" name="password" placeholder="密码" required>
    <button type="submit">登录</button>
    <div class="xxxx-div">
        <a href="index.html" class="xxxx-a">返回导航页</a>
    </div>
  </form>
  <p class="info-text">默认用户名：admin，密码：password123</p>
<?php endif; ?>
</div>
</body>
</html>