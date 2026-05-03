from setuptools import setup, find_packages

setup(
    name="crossplay-bot",
    version="0.1.0",
    description="Crossplay-playing AI with GADDAG-based move generation",
    author="Ali H Anjum",
    package_dir={"": "src"},
    packages=find_packages(where="src"),
    python_requires=">=3.8",
)
