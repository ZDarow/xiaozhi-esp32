"""Проверки согласованности docker-образа сборщика с репозиторием.

Образ не должен помечать себя чужим upstream: `FIRMWARE_SOURCE_URL` попадает
в OCI-лейблы и указывает, откуда на самом деле взяты исходники.
"""

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
DOCKERFILE = ROOT / "docker" / "firmware-builder" / "Dockerfile"


def remote_urls() -> set[str]:
    """Читает origin из .git/config без обращения к сети."""
    config = (ROOT / ".git" / "config").read_text(encoding="utf-8")
    return set(re.findall(r"url\s*=\s*(\S+)", config))


class DockerfileTest(unittest.TestCase):
    def test_source_url_matches_origin(self):
        """Метка происхождения должна совпадать с фактическим remote."""
        source = DOCKERFILE.read_text(encoding="utf-8")
        match = re.search(r"ARG\s+FIRMWARE_SOURCE_URL=(\S+)", source)
        self.assertIsNotNone(match, "Не найден ARG FIRMWARE_SOURCE_URL")

        declared = match.group(1).strip()
        origins = {url.removesuffix(".git") for url in remote_urls()}
        self.assertTrue(origins, "Не удалось прочитать origin из .git/config")
        self.assertIn(declared.removesuffix(".git"), origins,
                      f"FIRMWARE_SOURCE_URL={declared} не совпадает с origin {sorted(origins)}")

    def test_source_dir_comes_from_build_context(self):
        """Образ собирает исходники из контекста, а не клонирует репозиторий."""
        source = DOCKERFILE.read_text(encoding="utf-8")
        self.assertIn("COPY . .", source)
        self.assertNotIn("git clone", source)

    def test_builder_idf_version_supports_p4(self):
        """Образ должен собираться на IDF 6.1: P4/S31 требуют этой версии."""
        source = DOCKERFILE.read_text(encoding="utf-8")
        match = re.search(r"ARG\s+IDF_IMAGE=(\S+)", source)
        self.assertIsNotNone(match, "Не найден ARG IDF_IMAGE")
        self.assertRegex(match.group(1), r"6\.1|release-v6\.1")


if __name__ == "__main__":
    unittest.main()