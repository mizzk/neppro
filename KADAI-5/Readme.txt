課題5 TCP/UDP通信とブロードキャスト
21122051 MIZUTANI Kota
2023年7月9日

## 開発環境
WSL2 Ubuntu 22.04 LTS

## 使用法
./idobata -u username [-p port_number]

## 工夫した点
- main関数内での処理を少なくして可読性を高めるため，プログラムの開始処理，クライアントの処理，サーバの処理をそれぞれ関数化して呼び出すようにしている．
- サーバにおいて，UDPポートを用いた「HELO「HEREのやりとりと，TCPポートを用いた「JOIN」「POST」「MESG」「QUIT」のやりとりをスレッドを用いて独立した並列処理として実装している．そのた，たとえば「1回目のHEREを無視する」や「HEREを受け取った後connectしない」などの問題があるユーザに対して，サーバが正常に動作するようになっている．
- サーバにおいて，「connecした後JOINを送らない」ユーザに対しては，usernameを「unknown」として会議に参加できるようにした．
- サーバにおいて，長いusernameを送ってきたユーザに対しては，設定長を超えた部分を切り捨ててusernameを設定するようにした．
- サーバにおいて，メッセージを送るときに，POSTをつけなかったりMESGをつけるようなユーザのメッセージは無視されるようになっている．


## 苦労した点
- プログラムの開始処理を関数化する上で，変数の受け渡しについて苦労した．
- 開始処理において，タイムアウトと再送処理を組み合わせた処理を実装するための条件分岐の発案に時間がかかった．
- サーバにおいてスレッドを用いて実行させる関数の引数の受け渡しについて苦労した．
- selectを用いて複数のソケットを監視する処理と，キーボードからの入力を監視する処理を組み合わせることに苦労した．