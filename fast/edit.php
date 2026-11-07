<?php
// edit.php
session_start();

// 简单的认证凭据（在实际应用中应该使用更安全的方式）
$valid_username = "onno";
$valid_password = "Howtomakemoney1.";

// 要编辑的文件
$filename = "index.html";

// 初始化变量
$error = '';
$message = '';
$content = '';

// 处理登录
if (isset($_POST['action']) && $_POST['action'] == 'login') {
    if ($_POST['username'] == $valid_username && $_POST['password'] == $valid_password) {
        $_SESSION['authenticated'] = true;
    } else {
        $error = "用户名或密码错误";
    }
}

// 处理登出
if (isset($_GET['action']) && $_GET['action'] == 'logout') {
    session_destroy();
    header("Location: edit.php");
    exit;
}

// 处理保存
if (isset($_POST['action']) && $_POST['action'] == 'save' && isset($_SESSION['authenticated'])) {
    if (file_put_contents($filename, $_POST['content'])) {
        $message = "文件保存成功";
    } else {
        $error = "保存文件时出错";
    }
}

// 读取文件内容
if (isset($_SESSION['authenticated']) && file_exists($filename)) {
    $content = htmlspecialchars(file_get_contents($filename));
}
?>

<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>简单网页编辑器</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body { 
            font-family: Arial, sans-serif; 
            background-color: #f5f5f5; 
            padding: 20px 2%; 
        }
        .container { 
            width: 96%;
            max-width: none;
            margin: 0 auto; 
            background: white; 
            padding: 20px; 
            border-radius: 5px; 
            box-shadow: 0 2px 5px rgba(0,0,0,0.1); 
        }
        h1 { 
            color: #333; 
            text-align: center; 
            margin-bottom: 20px;
        }
        .login-form, .editor { 
            margin-bottom: 20px; 
        }
        input, textarea, button { 
            width: 100%; 
            padding: 10px; 
            margin: 5px 0; 
            border: 1px solid #ddd; 
            border-radius: 4px; 
        }
        button { 
            background-color: #4CAF50; 
            color: white; 
            border: none; 
            cursor: pointer; 
            font-size: 16px;
        }
        button:hover { 
            background-color: #45a049; 
        }
        .error { 
            color: red; 
            padding: 10px; 
            background-color: #ffebee; 
            border-radius: 4px; 
            margin-bottom: 15px;
        }
        .success { 
            color: green; 
            padding: 10px; 
            background-color: #e8f5e9; 
            border-radius: 4px; 
            margin-bottom: 15px;
        }
        .logout { 
            text-align: right; 
            margin-bottom: 10px; 
        }
        .logout a {
            color: #666;
            text-decoration: none;
        }
        .logout a:hover {
            text-decoration: underline;
        }
        textarea { 
            height: 70vh; 
            min-height: 400px;
            font-family: monospace; 
            font-size: 14px;
            line-height: 1.5;
        }
        .info-text {
            text-align: center; 
            font-size: 14px; 
            color: #666;
            margin-top: 10px;
        }
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
        
        <?php if (isset($_SESSION['authenticated'])): ?>
            <div class="logout">
                <a href="?action=logout">登出</a>
            </div>
            
            <?php if (!empty($message)): ?>
                <div class="success"><?php echo $message; ?></div>
            <?php endif; ?>
            
            <?php if (!empty($error)): ?>
                <div class="error"><?php echo $error; ?></div>
            <?php endif; ?>
            
            <form method="post" class="editor">
                <input type="hidden" name="action" value="save">
                <textarea name="content" placeholder="在此编辑index.html的内容..."><?php echo isset($content) ? $content : ''; ?></textarea>
                <button type="submit">保存更改</button>
            </form>
            
        <?php else: ?>
            <?php if (!empty($error)): ?>
                <div class="error"><?php echo $error; ?></div>
            <?php endif; ?>
            
            <form method="post" class="login-form">
                <input type="hidden" name="action" value="login">
                <input type="text" name="username" placeholder="用户名" required>
                <input type="password" name="password" placeholder="密码" required>
                <button type="submit">登录</button>
                <div class="xxxx-div">
                    <a href="index.html" class="xxxx-a">返回导航页</a>
                </div>
            </form>
            
            <p class="info-text">
                默认用户名: admin, 密码: password123
            </p>
        <?php endif; ?>
    </div>
</body>
</html>