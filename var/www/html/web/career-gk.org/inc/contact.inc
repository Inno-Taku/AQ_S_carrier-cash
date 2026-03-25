<?
require '/var/www/html/web/career-gk.org/vendor/autoload.php';
include_once("/var/www/html/web/career-gk.org/inc/user_status.php");

use Aws\Credentials\CredentialProvider;
use Aws\S3\Exception\S3Exception;
use Aws\CommandPool;

// iniファイルのパス
$path_s3      = '/var/www/html/web/career-gk.org/credentials_s3.ini';
$profile_s3   = 'default';
$bucket = "carrier-cash-uploadfile";
$tmp_bucket = "carrier-cash-tmpfile";

$_action = array(
    0 => "再入力をする",
    1 => "申し込み確認画面へ",
    2 => "申し込みを行う",
);
$_flip = array_flip($_action);
$action = $action ? $_flip[$action] : 0;

if (!empty($form["identification2"])) {
    $_SESSION['identification2'] = $form["identification2"];
}


// 申込ボタンが押されたとき
if ($action) {

    $site = $_domainsite[$domain2];

    $form["last_name2"] = mb_convert_kana($form["last_name2"], "KCV");
    $form["first_name2"] = mb_convert_kana($form["first_name2"], "KCV");

    // 変換
    $form["tel3"] = str_replace(array("ー", "-", "－", "―", "‐"), array("", "", "", "", ""), $form["tel3"]);
    $form["tel3"] = mb_convert_kana($form["tel3"], "a");

    if ($form["zipcode"]) {
        $form["zipcode1"] = substr($form["zipcode"], 0, 3);
        $form["zipcode2"] = substr($form["zipcode"], 3, 4);
    }

    @$form["zipcode1"] = mb_convert_kana($form["zipcode1"], "a");
    @$form["zipcode2"] = mb_convert_kana($form["zipcode2"], "a");
    $form["email"] = mb_convert_kana($form["email"], "a");
    $emailType = checkEmail($form["email"]);

    // エラーチェック
    if (empty($form["last_name1"]) || empty($form["first_name1"])) $err[] = "名前 を入力してください";
    if (empty($form["last_name2"]) || empty($form["first_name2"])) $err[] = "フリガナ を入力してください";

    if (preg_match("/^[ァ-ヾ]+$/u", $form["last_name1"]) || preg_match("/^[ァ-ヾ]+$/u", $form["first_name1"])) $err[] = "お名前は、ひらがなと漢字のみでご入力してください";
    if (!preg_match("/^[ぁ-んァ-ヶー一-龠々]+$/u", $form["last_name1"]) || !preg_match("/^[ぁ-んァ-ヶー一-龠々]+$/u", $form["first_name1"])) $err[] = "お名前は、ひらがなと漢字のみでご入力してください";
    if ((!empty($form["last_name2"]) and !mb_ereg("^[　 ァ-ンー]+$", $form["last_name2"])) || (!empty($form["first_name2"]) and !mb_ereg("^[　 ァ-ンー]+$", $form["first_name2"]))) $err[] = "フリガナ はカタカナで入力してください";

    if (empty($form["tel3"])) $err[] = "携帯番号 を入力してください";
    if (!empty($form["tel3"]) and (!ctype_digit($form["tel3"]) or (strlen($form["tel3"]) != 11))) $err[] = "携帯番号 を正しく入力してください";
    if (!empty($form["tel3"])) {
        $rst = $mysqli->query("SELECT uid FROM `user` WHERE `tel3` LIKE '{$form["tel3"]}' and site = 1");
        // ユーザーの電話番号とフォームから送信された電話番号を比較してエラーを生成
        if (mysqli_num_rows($rst) != 0) {
            $err[] = "※すでにご登録がある電話番号の為、マイページにログインを行いお申込みください。";
        }
    }

    if ($emailType == 3) $err[] = "メールアドレス を入力してください";
    if ($emailType == 4) $err[] = "メールアドレス を正しく入力してください";
    $dnsCheck = explode("@", $form["email"]);
    if (!(checkdnsrr($dnsCheck[1]))) {
        $err[] = "メールアドレスのドメイン部分が不正です";
    }
    if (!empty($form["email"])) {
        $rst = $mysqli->query("SELECT uid FROM `user` WHERE (`email1` = '{$form["email"]}' OR `email2` = '{$form["email"]}') and site = 1");
        // ユーザーのメールアドレスとフォームから送信されたメールアドレスを比較してエラーを生成
        if (mysqli_num_rows($rst) != 0) {
            $err[] = "※すでにご登録があるメールアドレスの為、マイページにログインを行いお申込みください。";
        }
    }

    if (empty($form["birthdate"])) $err[] = "生年月日 を入力してください";
    list($birth_y, $birth_m, $birth_d) = explode("-", $form["birthdate"]);

    if (!checkdate($birth_m, $birth_d, $birth_y)) $err[] = "生年月日 を正しく入力してください";

    if (calc_age($form["birthdate"]) < 18) {
        $err[] = "18歳未満は登録できません";
    }

    if (empty($form["password"])) $err[] = "パスワード を入力してください";
    if (empty($form["password2"])) $err[] = "パスワード(確認用) を入力してください";
    if ($form["password"] != $form["password2"]) $err[] = "パスワードとパスワード(確認用) は同じものを入力してください";
    if (empty($form["password"])) $err[] = "パスワード を入力してください";
    if (!preg_match("/\A[a-z\d]{8,12}+\z/i", $form["password"])) $err[] = "パスワードは半角英数字で8文字以上12文字以内で入力してください";
    if ($form["addr1"] == 99 or !isset($_addr1[$form["addr1"]])) $err[] = "都道府県を選択してください";
    if (empty($form["addr2"])) $err[] = "市町村番地を入力してください";

    if (empty($form["carrier"])) $err[] = "キャリアを選択してください";

    if (empty($form["desiredamount"])) $err[] = "ご利用希望金額 を入力してください";


    //申込金額
    $what_application = 1;
    $form["payment_method_type"] = 2;
    include_once("/var/www/html/web/career-gk.org/inc/transaction_limits.inc");

    //お振込み情報
    if (empty($form["bank_name"])) $err[] = "金融機関名 を入力してください";
    if (empty($form["bank_branch_name"])) $err[] = "支店名 を入力してください";
    if (empty($form["bank_account_no"])) $err[] = "口座番号 を入力してください";
    if (empty($form["bank_account_type"])) $err[] = "口座種別 を選択してください";
    // if(empty($form["bank_account_name"])) $err[] = "口座名義 を入力してください";
    if (empty($form["bank_account_name_kana"])) $err[] = "口座名義(カナ) を入力してください";
    if (!empty($form["bank_account_name_kana"]) and !mb_ereg("^[　 ァ-ンー]+$", $form["bank_account_name_kana"])) $err[] = "口座名義（カナ） はカタカナで入力してください";

    // if ($_FILES) {
    //     $allowedTypes = array('image/png', 'image/jpeg', 'image/jpg', 'image/gif');

    //     foreach ($_FILES as $key => $fileCheck) {


    //         if (!in_array($fileCheck['type'], $allowedTypes)) {
    //             if ($key == "file1") {
    //                 $fileErrorName = "IDセルフィー";
    //             } else if ($key == "file2") {
    //                 $fileErrorName = "本人確認書類＜表＞";
    //             } else if ($key == "file3") {
    //                 $fileErrorName = "本人確認書類＜裏＞";
    //             }
    //             $err[] = "「" . $fileErrorName . "」 は違うファイル形式です。PNG、JPG、JPEG、およびGIFファイルのみをアップロードしてください。";
    //         }
    //     }
    // }

    // エラー表示
    if (count($err)) {
        $msg = showerr($err);
        $action = 0;
        // エラーがあった場合はLocationで戻す為セッションに格納する
        if (in_array($site, array(1, 2))) {
            $_SESSION['errorMessageList'] = $err;
            $_SESSION['formValueList'] = $form;
            header('Location: ./contactInput.php');
            exit();
        }
        //確認
    } elseif ($action == 1) {
        //仮のフォルダにアップ処理ここから

        // iniファイルでの認証するための処理
        $provider_s3 = CredentialProvider::ini($profile_s3, $path_s3);
        $provider_s3 = CredentialProvider::memoize($provider_s3);

        $s3 = Aws\S3\S3Client::factory([
            'version' => 'latest',
            'region'  => 'ap-northeast-1',
            'credentials' => $provider_s3
        ]);
        // //そのチケットのフォルダーがあるかチェック
        // $list_objects = $s3->listObjects(array(
        //     'Bucket'  => $tmp_bucket,
        //     'Prefix'  => $_SESSION['ticket'],
        //     'MaxKeys' => '1'
        // ));

        // //見つからなければ、作成
        // if (is_null($list_objects['Contents'])) {
        //     $s3_create = $s3->putObject([
        //         'ACL' => 'public-read',
        //         'Bucket' => $tmp_bucket,
        //         'Key' =>  $_SESSION['ticket']."/",
        //     ]);
        // }    

        $batch = array();
        //         echo substr( sprintf( '%o', fileperms("/var/www/html/web/".$domain2."/tmp_attachment/")), -4);
        //         $user = exec('whoami');
        // $group = exec('groups ' .$user);
        // echo "ユーザー:{$user}<br>";
        // echo "グループ:{$group}<br>";
        //         exit;

        //s3へのアップロード
        if ($dispCIimg == 1) {
            $filecount = 4;
        } elseif ($dispCIimg == 2) {
            $filecount = 3;
        }
        for ($jjj = 1; $jjj <= $filecount; $jjj++) {

            if (!isset($_FILES['file' . $jjj])) {
                continue;
            }

            // オリジナル画像のファイルパスを指定
            $org_file = $_FILES['file' . $jjj]['tmp_name'];
            if (!$org_file) {
                continue;
            }

            list($org_w, $org_h, $ext_num) = getimagesize($org_file);
            $resizeWidth = 640;
            // 縦横のリサイズ後のピクセル数を求める
            $ratio = $resizeWidth / $org_w;
            $copy_w = $org_w * $ratio;
            $copy_h = $org_h * $ratio;

            if ($ext_num == "4") {
                // オリジナルファイルの画像リソース
                $org_img = imagecreatefrompng($org_file);
            } elseif ($ext_num == "3") {
                // オリジナルファイルの画像リソース
                $org_img = imagecreatefrompng($org_file);
            } elseif ($ext_num == "2") {
                // オリジナルファイルの画像リソース
                $org_img = imagecreatefromjpeg($org_file);
            } else {
                // オリジナルファイルの画像リソース
                $org_img = imagecreatefromgif($org_file);
            }
            // コピー画像のリソース
            $copy_img = imagecreatetruecolor($copy_w, $copy_h);

            // リサイズ
            imagecopyresampled($copy_img, $org_img, 0, 0, 0, 0, $copy_w, $copy_h, $org_w, $org_h);

            // 拡大したコピー画像を表示
            //      imagejpeg($copy_img);
            $attachemnt_path = "/var/www/html/web/" . $domain2 . "/tmp_attachment/";
            $new_name = $_SESSION['ticket'] . "-" . $jjj;
            $org_dirFile = "{$attachemnt_path}" . $new_name;

            if ($ext_num == "4") {
                imagepng($copy_img, $org_dirFile, 3);
            } elseif ($ext_num == "3") {
                imagepng($copy_img, $org_dirFile, 3);
            } elseif ($ext_num == "2") {
                imagejpeg($copy_img, $org_dirFile, 70);
            } else {
                imagegif($copy_img, $org_dirFile);
            }
            // 画像リソースを破棄
            imagedestroy($org_img);
            imagedestroy($copy_img);



            // putObject用のコマンドを生成して配列に格納
            $batch[] = $s3->getCommand('PutObject', [
                'Bucket' => $tmp_bucket,
                'Key' => $_SESSION['ticket'] . "/" . $new_name,
                // 'SourceFile' => $_FILES['file'.$jjj]['tmp_name'],
                // 'ContentType' => $_FILES['file'.$jjj]['type'],
                'SourceFile' => $org_dirFile
            ]);
        }
        // 同時コマンド実行！
        CommandPool::batch($s3, $batch);

        //tmp_attachementを削除
        for ($jjj = 1; $jjj <= $filecount; $jjj++) {
            $new_name = $ticket . "-" . $jjj;
            $org_dirFile = "{$attachemnt_path}/" . $new_name;
            //tmpは使用しないのでファイルを削除
            if (file_exists($org_dirFile)) {
                //unlink($org_dirFile);
            }
        }

        //各画像をセッションに保存(確認ページ表示用)
        if ($_FILES['file1']['tmp_name']) {
            $_SESSION['file1']['data'] = file_get_contents($_FILES['file1']['tmp_name']);
        }
        if ($_FILES['file1']['tmp_name']) {
            $_SESSION['file1']['type'] = exif_imagetype($_FILES['file1']['tmp_name']);
        }
        if ($_FILES['file2']['tmp_name']) {
            $_SESSION['file2']['data'] = file_get_contents($_FILES['file2']['tmp_name']);
        }
        if ($_FILES['file2']['tmp_name']) {
            $_SESSION['file2']['type'] = exif_imagetype($_FILES['file2']['tmp_name']);
        }
        if (isset($_FILES['file3']['tmp_name']) && $_FILES['file3']['tmp_name']) {
            $_SESSION['file3']['data'] = file_get_contents($_FILES['file3']['tmp_name']);
        }
        if (isset($_FILES['file3']['tmp_name']) && $_FILES['file3']['tmp_name']) {
            $_SESSION['file3']['type'] = exif_imagetype($_FILES['file3']['tmp_name']);
        }
        if ($dispCIimg == 1) {
            if ($_FILES['file4']['tmp_name']) {
                $_SESSION['file4']['data'] = file_get_contents($_FILES['file4']['tmp_name']);
            }
            if ($_FILES['file4']['tmp_name']) {
                $_SESSION['file4']['type'] = exif_imagetype($_FILES['file4']['tmp_name']);
            }
        }

        // 申込
    } elseif ($action == 2) {

        //メール添付用
        $attachment = array();
        $attachmentImage = array();
        foreach ($ticketFiles[$ticket] as $key => $val) {
            // オリジナル画像のファイルパスを指定

            // 拡大したコピー画像を表示
            //      imagejpeg($copy_img);
            $attachemnt_path = "/var/www/html/web/"  . "{$domain2}/tmp_attachment";

            $new_name = $val;
            $org_dirFile = "{$attachemnt_path}/" . $new_name;

            // ファイルインフォデータベースを開く
            $finfo = finfo_open(FILEINFO_MIME_TYPE);
            $mime = finfo_file($finfo, $org_dirFile);
            @$cfile[$key] = new CURLFile($org_dirFile, $mime);

            // ファイルインフォデータベースを閉じる
            finfo_close($finfo);


            //配列にセット
            $attachmentImage["file{$key}"] = $org_dirFile;
            $delfile[] = $org_dirFile;
        }

        if ($attachmentImage) {
            $attachmentCnt = 0;
            foreach ($attachmentImage as $key => $val) {


                $pathParts = explode("/", $val);
                $fileName = end($pathParts);

                //添付ファイルの
                $size = getimagesize($val);
                $attachment[$attachmentCnt]["linkname"] = $val;
                $attachment[$attachmentCnt]["mimetype"] = $size["mime"];
                $attachment[$attachmentCnt]["filename"] = $fileName;


                $attachmentCnt++;
            }
        }

        // チケットを確認
        if (isset($_POST['ticket'])  &&  isset($_SESSION['ticket'])) {
            $ticket = $_POST['ticket'];
            if ($ticket !== $_SESSION['ticket']) {
                $err = array("申し訳ございませんが、最初からやり直してください。");
                $_SESSION['errorMessageList'] = $err;
                header('Location: errorRegist.php');
                exit();
            }
        } else {
            $err = array("申し訳ございませんが、最初からやり直してください。");
            $_SESSION['errorMessageList'] = $err;
            header('Location: errorRegist.php');
            exit();
        }

        $access_key = md5(uniqid(rand(), 1));
        $user['device'] = isset($form['device']) ? $form['device'] : 1;
        $uid = 0;
        $user["access_key"] = $access_key;
        $user["password"] = $form["password"];
        $user["regdate"] = date("Y-m-d");
        $user["regdatetime"] = date("Y-m-d H:i:s");
        $history["applicationdate"] = date("YmdHis");
        $user["site"] = $_domainsite[$domain2];
        $user["name1"] = $form["last_name1"] . $form["first_name1"];
        $user["name2"] = $form["last_name2"] . $form["first_name2"];
        $user["last_name1"] = $form["last_name1"];
        $user["first_name1"] = $form["first_name1"];
        $user["last_name2"] = $form["last_name2"];
        $user["first_name2"] = $form["first_name2"];
        $user["birthdate"] = $form["birthdate"];
        $user["email{$emailType}"] = $form["email"];
        $user["tel3"] = $form["tel3"];
        $user["zipcode1"] = isset($form["zipcode1"]) ? $form["zipcode1"] : "";
        $user["zipcode2"] = isset($form["zipcode2"]) ? $form["zipcode2"] : "";
        $user["addr1"] = $form["addr1"];
        $user["addr2"] = $form["addr2"];
        $user["addr3"] = $form["addr3"];
        $user["identification2"]  = isset($form["identification2"]) ? $form["identification2"] : "99";
        // $user["desiredamount"] = $form["desiredamount"]."000";
        $user["ip"] = @$_SERVER["HTTP_X_FORWARDED_FOR"];
        $user["useragent"] = @$_SERVER['HTTP_USER_AGENT'];
        $user["host"] = @$_SERVER['REMOTE_HOST'];

        //お振込み情報
        $bank[1] = array();
        $bank[1]["name1"] = $form["bank_name"];
        $bank[1]["name2"] = $form["bank_branch_name"];
        $bank[1]["account_type"] = $form["bank_account_type"];
        $bank[1]["number"] = $form["bank_account_no"];
        // $bank[1]["holder"] = $form["bank_account_name"];
        $bank[1]["holder_kana"] = $form["bank_account_name_kana"];

        $user["bank"] = serialize($bank);


        // メモ1対応
        $memoformat1 = '';
        $spot = 1;

        $rst = $mysqli->query("select * from `memoformat` WHERE spot = " . $spot);
        if (mysqli_num_rows($rst)) {
            $memoformat = mysqli_fetch_assoc($rst);
            if ($memoformat["body"] != '') {
                $memoformat1 = str_replace("\r\n", "\n", $memoformat["body"]);
            } else {
                $memoformat1 = str_replace("\r\n", "\n", $memo1Default);
            }
        } else {
            $memoformat1 = str_replace("\r\n", "\n", $memo1Default);
        }

        // cidの取得
        $rst = $mysqli->query("select cid from code where name = '{$code}'");

        if ($cid = mysqli_fetch_assoc($rst)) {
            $user["cid"] = $cid["cid"];
            $user["cid_access"] = $cid["cid"];
        } else {
            $user["cid"] = "1000000001";
            $user["cid_access"] = "1000000001";
        }

        if (isset($_COOKIE["gclid"])) {
            $user["gclid"] = $_COOKIE["gclid"];
        }

        require_once("check_duplicate_user.inc");
        $duplicateCid = getDuplicateCidByEmailOrTel($mysqli, $form["email"], $form["tel3"]);
        if ($duplicateCid !== null) {
            $user["cid"] = $duplicateCid;
        }

        // 新規であればデータベースへ追加
        $rst = $mysqli->query("select uid,access_key from user where site = {$site} and email{$emailType} = '{$form['email']}'");
        $uidForPrgRecalc = null;

        if (!mysqli_num_rows($rst)) {
            $user["ngflag"] = 3;
            $uid = $user["uid"] = mysql_insert("user", $user);
            fusion_insert_user($uid, $service_id[$siteId]);

            $uidForPrgRecalc = $uid;
            $new = 1;
        } else {
            $checkUserData = mysqli_fetch_assoc($rst);
            $uid = $user["uid"] = $checkUserData["uid"];
            $mysqli->query("update user set ngflag = 3 where uid = {$uid}");
            fusion_update_user($uid, $service_id[$siteId]);

            $new = 0;
            $user["access_key"] = $checkUserData["access_key"];
        }


        //新規の場合
        if ($new) {
            //ユーザーデータを再取得
            $rst = $mysqli->query("select access_key from user where uid = '{$uid}'");
            $userData = mysqli_fetch_assoc($rst);

            // iniファイルでの認証するための処理
            $provider_s3 = CredentialProvider::ini($profile_s3, $path_s3);
            $provider_s3 = CredentialProvider::memoize($provider_s3);

            $s3 = Aws\S3\S3Client::factory([
                'version' => 'latest',
                'region'  => 'ap-northeast-1',
                'credentials' => $provider_s3
            ]);

            //仮のパケットから、アップロード画像を取得しする
            //そのユーザーのフォルダーがあるかチェック
            $tmplist_objects = $s3->listObjects(array(
                'Bucket'  => $tmp_bucket,
                'Prefix'  => $ticket,
            ));

            // if (!is_null($tmplist_objects['Contents'])) {

            //     //そのユーザーのフォルダーがあるかチェック
            //     $list_objects = $s3->listObjects(array(
            //         'Bucket'  => $bucket,
            //         'Prefix'  => $userData["access_key"],
            //     ));
            //     //見つからなければ、作成
            //     if (is_null($list_objects['Contents'])) {
            //         $s3_create = $s3->putObject([
            //             'ACL' => 'public-read-write',
            //             'Bucket' => $bucket,
            //             'Key' =>  $userData["access_key"].'/',
            //         ]);
            //     }    
            $batch = array();
            //コピー処理
            foreach ($tmplist_objects['Contents'] as $file) {
                $batch[] = $s3->getCommand('CopyObject', [
                    'Bucket'  => $bucket, //バケットBを指定
                    'Key' =>  $userData["access_key"] . '/' . $file['Key'],
                    'CopySource' => $tmp_bucket . '/' . $file['Key'], //xxxxxxはバケットAを指定

                ]);
            }


            //一括でコピー
            try {
                $results = CommandPool::batch($s3, $batch);
            } catch (CommandTransferException $e) {
                $succeess = $e->getSuccessfulCommands();
                echo "Failed Commands:\n";
                foreach ($e->getFailedCommands() as $failedCommand) {
                    echo $e->getExceptionForFailedCommand($failedCommand)->getMessage() . "\n";
                }
                exit();
            }
            // }

            $desiredamount = $form["desiredamount"] . "000";

            //利用履歴のインサート
            $history = array();
            $history["uid"] = $uid;
            $history["desiredamount"] = $desiredamount;
            $history["site"] = $site;
            $history["stepstatus"] = 1;
            $history["status"] = 8; // 申込ステータス　8:本人確認待ち
            // $history["status"] = 1;
            $history["applicationdate"] = date("YmdHis");
            $history["opid"] = "1000000001";
            $history["bank"] = "1";
            $history["type"] = 3;
            $history["carrier"] = $form["carrier"];
            $history["phone"] = $form["tel3"];
            // $history["spid"] = EC_SHOP_ID_TREASURESTORE;
            $history["spid"] = 1000000002;
            $history["payment_method_type"] = 2;
            $history["purchase_status"] = 1; //購入ステータス　新規の場合は確認済み

            //自動オート
            include_once("/var/www/html/web/career-gk.org/inc/auto.inc");

            $htid = mysql_insert("history", $history);
            fusion_insert_history($htid, $service_id[$site]);



            //本人確認書類ファイルのログインサイト
            $identificationfile = array();
            $identificationfile["uid"] = $uid;
            $identificationfile["htid"] = $htid;
            $identificationfile["ticket"] = $ticket;
            $identificationfile["uploaddatetime"] = date("YmdHis");
            mysql_insert("identificationfile", $identificationfile);
        }

        if ($uidForPrgRecalc) {
            $userStatus = new UserStatusService($uid, $mysqli);
            $decision = $userStatus->decidePrgStatusForUidUsingExistingFast((int)$uid);
            if (!is_null($decision['resolved_prg']) && $decision['resolved_prg'] !== ($decision['current_prg'] ?? null)) {
                $ok = $userStatus->updatePrgStatus(
                    (int)$uid,
                    $decision['resolved_prg'],
                    'キャリアキャッシュ新規申込_自動判定',
                    '/var/www/html/web/career-gk.org/inc/contact.php'
                );

                if ($ok) {
                    $user['prg_status'] = $decision['resolved_prg'];
                }
            }
        }

        // 和暦変換
        function convertToWareki($date)
        {
            $timestamp = strtotime($date);
            if ($timestamp === false) {
                return '';
            }

            $year = (int)date('Y', $timestamp);
            $month = (int)date('m', $timestamp);
            $day = (int)date('d', $timestamp);

            // 元号判定
            if ($year > 2019 || ($year === 2019 && $month >= 5)) {
                $era = '令和';
                $warekiYear = $year - 2018; // 令和1年 = 2019年
            } elseif ($year > 1989 || ($year === 1989 && $month >= 1 && $day >= 8)) {
                $era = '平成';
                $warekiYear = $year - 1988; // 平成1年 = 1989年
            } else {
                $era = '昭和';
                $warekiYear = $year - 1925; // 昭和1年 = 1926年
            }

            return "{$era}{$warekiYear}年{$month}月{$day}日";
        }
        $wareki = convertToWareki($form["birthdate"]);

        $ageText = '';
        if (!empty($form["birthdate"])) {
            try {
                $bd = trim((string)$form["birthdate"]);

                if (preg_match('/^\d{8}$/', $bd)) {
                    $birth = DateTime::createFromFormat('Ymd', $bd);
                } elseif (preg_match('/^\d{4}\/\d{2}\/\d{2}$/', $bd)) {
                    $birth = DateTime::createFromFormat('Y/m/d', $bd);
                } else {
                    $birth = new DateTime($bd);
                }

                if ($birth instanceof DateTime) {
                    $today = new DateTime('today');
                    $age = $birth->diff($today)->y;
                    $ageText = "（{$age}歳）";
                }
            } catch (Exception $e) {
                $ageText = '';
            }
        }

        // 運営者へメールを送信
        $subject = "{$form['last_name1']} {$form['first_name1']}様：{$servicename}：申込がありました";
        $text = "下記の内容でお申し込みがありました。\n\n";

        if (!empty($uid)) $text .= "uid:{$uid}\n";
        if (!empty($htid)) $text .= "htid:{$htid}\n";
        if (!empty($form["last_name1"]) && !empty($form["first_name1"])) $text .= "お名前：{$form['last_name1']} {$form['first_name1']}\n";
        if (!empty($form["last_name2"]) && !empty($form["first_name2"])) $text .= "フリガナ：{$form['last_name2']} {$form['first_name2']}\n";
        if (isset($form["birthdate"])) $text .= "生年月日：{$form['birthdate']}（{$wareki}）{$ageText}\n";
        if (!empty($form["email"])) $text .= "メールアドレス：{$form['email']}\n";
        if (!empty($form["tel3"])) $text .= "電話番号：{$form['tel3']}\n";
        if (isset($form["zipcode1"])) $text .= "郵便番号：{$form['zipcode1']} - {$form['zipcode2']}\n";
        if (isset($_addr1[$form["addr1"]])) $text .= "都道府県：{$_addr1[$form['addr1']]}\n";
        if (!empty($form["addr2"])) $text .= "住所：{$form['addr2']}\n";
        if (!empty($form["addr3"])) $text .= "番地、建物名：{$form['addr3']}\n";
        if (!empty($form["carrier"])) $text .= "キャリアの選択：{$_mobile_carriers[$form['carrier']]}\n";
        if (!empty($form["desiredamount"])) $text .= "ご利用希望金額：" . number_format($form['desiredamount']) . ",000円\n";


        //お振込み情報
        if (!empty($form["bank_name"])) $text .= "金融機関名：{$form['bank_name']}\n";
        if (!empty($form["bank_branch_name"])) $text .= "支店名：{$form['bank_branch_name']}\n";
        if (!empty($form["bank_account_type"])) $text .= "口座種別：{$_passbook_account_type[$form['bank_account_type']]}\n";
        if (!empty($form["bank_account_no"])) $text .= "口座番号：{$form['bank_account_no']}\n";
        // if(!empty($form["bank_account_name"])) $text .= "口座名義人：{$form['bank_account_name']}\n";
        if (!empty($form["bank_account_name_kana"])) $text .= "口座名義人（カナ）：{$form['bank_account_name_kana']}\n";


        $text .= "キャンペーンコード：{$code}\n";
        sendmail_ses("info@{$domain2}", $subject, $text, "info@{$domain2}", "", "", "", $attachment, "");


        // 申し込み回数カウント用のテーブルへ
        $exInfo = ", device = {$user['device']} , site = {$user['site']}";
        $mysqli->query("insert into contact set email = '{$form['email']}', cid = {$user['cid']}, date = '{$user['regdate']}' $exInfo ");

        // リメール
        $user["email"] = $form["email"];

        $user["desiredamount"] = $form["desiredamount"];

        if ($new) {
            $rmid = 201;
            //重複メール
        } else {
            $rmid = 102;
        }
        sendremail_ses($rmid, $user);


        //処理が終了とともにattachementファイルを削除
        if (!empty($delfile)) {
            foreach ($delfile as $delfileval) {
                unlink($delfileval);
            }
        }

        //セッションを破棄
        session_destroy();
    }
}
