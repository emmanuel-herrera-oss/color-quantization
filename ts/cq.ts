export {}
import { Jimp, intToRGBA, rgbaToInt } from "jimp";
import { kmeans } from "./kmeans";
import { getInput } from "./inp";

const input = getInput()
console.log(`Reading ${input.from}...`)
const image = await Jimp.read(input.from);
const map = new Map<number[], number[]>()
const data: number[][] = []
for(let y = 0;y < image.height;y++) {
    for(let x = 0;x < image.width;x++) {
        const {r, g, b} = intToRGBA(image.getPixelColor(x, y))
        const rgb = [r, g, b]
        map.set(rgb, [x, y])
        data.push(rgb)
    }
}
const k = input.clusters
console.log('Clustering is starting.')
const start = new Date().getTime()
const result = kmeans(data, k, input.iter)
const end = new Date().getTime()
console.log(`Finished clustering ${(image.height * image.width).toLocaleString('en')} pixels in ${end - start} milliseconds. (${result.iterationCount} iterations) Storing image...`)
for(let i = 0;i < k;i++) {
    for(let j = 0;j < result.clusters[i].length;j++) {
        const rgb = result.clusters[i][j]
        const xy = map.get(rgb)
        if(!xy) {
            throw new Error('Map did not contain xy.')
        }
        image.setPixelColor(
            rgbaToInt(
                result.centroids[i][0],
                result.centroids[i][1], 
                result.centroids[i][2],
                255
            ),
            xy[0],
            xy[1]
        )
    }
}
const toPath = input.to.split('.')
await image.write(`${toPath[0]}.${toPath[1]}`); //Requires us to pass a string like `${0}.${1}`...interesting.
console.log(`Finished. Saved result to: ${input.to}`)