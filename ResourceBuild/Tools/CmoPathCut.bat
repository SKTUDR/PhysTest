rem モデルデータのフォルダ
set MODEL=Models

rem CMOの出力フォルダ
set CMO=CMO

rem CMOのリソースフォルダ
set OUT=Resources\Models

rem cmoの出力先フォルダを作成
if not exist %1"%CMO%" mkdir %1"%CMO%"

rem ddsをcmoの出力先へコピー
xcopy %1"*.dds" %1"%CMO%" /Y

rem csoをcmoの出力先へコピー
xcopy %1"*.cso" %1"%CMO%" /Y

rem 元のddsは削除
del %1"*.dds"

rem 今は使わないのでcsoも削除
del %1"*.cso"

rem cmoの中のddsファイル名を変更と、ddsファイル名を変更
"%~dp0"CmoPathCut.exe /m "%MODEL%" /c "%CMO%"

rem コンバート失敗時の不要ファイルを削除
del %1"%CMO%\*.new"
del %1"%CMO%\*.cso"

rem リソースフォルダを作成
rem if not exist %1"%OUT%" mkdir %1"%OUT%"

rem コンバートしたcmoとddsをリソースフォルダへコピー
rem xcopy %1"%CMO%" %1"%OUT%" /Y

