import os, zipfile, shutil
import urllib3

http_url = "http://erinrv.qscalp.ru/"

secur_list = []
with open('./secur.txt') as f:
    secur_list = f.read().splitlines()
data_list = []
with open('./data.txt') as f:
    data_list = f.read().splitlines()

if not os.path.isdir("./!zip"):
    os.mkdir("./!zip")

http = urllib3.PoolManager()

def download_file(url, outFileName):
    if os.path.isfile(outFileName):
        return True
    with http.request('GET', url, preload_content=False) as r, open(outFileName, 'wb') as out_file:
        if r.status == 404:
            print("error load: " + url)
            return False
        else:
            shutil.copyfileobj(r, out_file)
            return True

def download_securities(secur):
    print("start load " + secur)
    if not os.path.isdir("./"+secur):
        os.mkdir("./"+secur)
    print("")
    for d in data_list:
        print("\rdownload: " + d + "\r", end="")
        url = http_url+d+"/"+secur+"."+d
        file = "./"+secur+"/"+d;
        ok = download_file(url+".AuxInfo.qsh", file+".AuxInfo.qsh")
        ok &= download_file(url+".Deals.qsh", file+".Deals.qsh")
        ok &= download_file(url+".Quotes.qsh", file+".Quotes.qsh")
        if not ok:
            if os.path.isfile(file+".AuxInfo.qsh"):
                os.remove(file+".AuxInfo.qsh")
            if os.path.isfile(file+".Deals.qsh"):
                os.remove(file+".Deals.qsh")
            if os.path.isfile(file+".Quotes.qsh"):
                os.remove(file+".Quotes.qsh")
            print("delete: "+file+"*")
            print("")
    file_list = os.listdir("./"+secur)
    print("create: "+secur+".zip          ")
    zipFile = zipfile.ZipFile("./!zip/"+secur+".zip", "w", zipfile.ZIP_STORED)
    for f in file_list:
        zipFile.write("./"+secur+"/"+f, f)
    zipFile.close()
#
for s in secur_list:
    download_securities(s)
