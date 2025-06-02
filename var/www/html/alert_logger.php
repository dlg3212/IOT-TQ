<?php
if (isset($_GET["msg"])) {
    $msg = htmlspecialchars($_GET["msg"]);

    // 로그 파일에 저장
    $logfile = fopen("alerts.log", "a");
    fwrite($logfile, "[" . date("Y-m-d H:i:s") . "] " . $msg . "\n");
    fclose($logfile);

    // JSON 응답
    echo json_encode(["status" => "ok", "received" => $msg]);
} else {
    echo json_encode(["status" => "error", "message" => "No message received"]);
}
?>