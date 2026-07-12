"""分析 Transformer 模型余弦相似度"""
import onnxruntime as ort
import numpy as np
import sentencepiece as spm
import tempfile, os, shutil

src = r'F:\LianCore软音源合成器V3版本商业正式版\models\tokenizer\tokenizer.model'
tmp = tempfile.mkdtemp(prefix='sp_')
tmp_model = os.path.join(tmp, 'tokenizer.model')
shutil.copy2(src, tmp_model)
sp = spm.SentencePieceProcessor()
sp.load(tmp_model)

session = ort.InferenceSession(r'F:\LianCore软音源合成器V3版本商业正式版\models\transformer_encoder.onnx')

texts = [
    '温暖的贝斯', '尖锐的失真音色', '梦幻的铺底',
    '厚重的打击乐器', '快的弹拨合成器', '空灵的环境音效',
    '带一点点混响的现代贝斯', '温暖的合成器贝斯音色',
    '明亮的复古主音', '非常白的噪音', '极具攻击性的金属',
    '柔和的钢琴', '深沉的弦乐', '明亮的铜管',
    '湿润的合唱效果', '干的信号', '长延迟',
    '无混响', '大混响', '极快起音',
]
MAX_LEN = 64

input_ids = np.zeros((len(texts), MAX_LEN), dtype=np.int64)
for i, t in enumerate(texts):
    ids = sp.encode(t, out_type=int)[:MAX_LEN]
    input_ids[i, :len(ids)] = ids

emb = session.run(None, {'input_ids': input_ids})[0]

def cos(a, b):
    return np.dot(a, b) / (np.linalg.norm(a) * np.linalg.norm(b) + 1e-9)

print('=== Cosine Similarity Analysis ===')
print(f'Embedding dimension: {emb.shape[1]}')
print(f'Mean embedding norm: {np.mean(np.linalg.norm(emb, axis=1)):.4f}')
print(f'Std of norms: {np.std(np.linalg.norm(emb, axis=1)):.4f}')
print()

sims = []
for i in range(len(texts)):
    for j in range(i+1, len(texts)):
        s = cos(emb[i], emb[j])
        sims.append(s)

sims = np.array(sims)
print(f'Min cosine similarity: {sims.min():.4f}')
print(f'Max cosine similarity: {sims.max():.4f}')
print(f'Mean cosine similarity: {sims.mean():.4f}')
print(f'Std cosine similarity: {sims.std():.4f}')
print()

pairs = [(cos(emb[i], emb[j]), i, j) for i in range(len(texts)) for j in range(i+1, len(texts))]
pairs.sort(reverse=True)
print('Top 5 most similar pairs:')
for s, i, j in pairs[:5]:
    print(f'  {s:.4f}: "{texts[i]}" <-> "{texts[j]}"')

pairs.sort()
print('Top 5 most dissimilar pairs:')
for s, i, j in pairs[:5]:
    print(f'  {s:.4f}: "{texts[i]}" <-> "{texts[j]}"')

shutil.rmtree(tmp, ignore_errors=True)