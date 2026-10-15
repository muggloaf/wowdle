from pathlib import Path

from wordfreq import word_frequency


words_file = Path("data/wordlist_fives.txt")
frequency_file = Path("data/word_frequencies_fives.txt")
words = words_file.read_text().split()

lines = []
for word in words:
    word = word.lower()
    frequency = word_frequency(word, "en")
    lines.append(f"{word} {frequency:.12g}")

frequency_file.write_text("\n".join(lines) + "\n")
print(f"wrote {len(lines)} frequencies to {frequency_file}")
