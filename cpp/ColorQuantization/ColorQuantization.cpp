#include <iostream>
#include "lodepng.h"
#include <array>
#include <vector>
#include <math.h>
#include <chrono>
#include "CLI11.hpp"

struct QuantizeOptions {
    unsigned char* image;
    unsigned int image_len;
    unsigned int cluster_count = 8;
    unsigned int iteration_limit = 1000;
};

static unsigned int kmeans(const QuantizeOptions& options);
static void init_centroids(float** centroids, const QuantizeOptions& options);
static void init_clusters(std::vector<unsigned char*>* clusters, const QuantizeOptions& options);
static bool update_centroids(float** centroids, const std::vector<unsigned char*>* clusters, const QuantizeOptions& options);
static float distance_from_centroid(const float* centroid, const unsigned char* point);

int main(int argc, char** argv)
{
    // Options
    CLI::App app{ "This program maps the colors in a png image to a lower number of colors using the k-means algorithm." };
    argv = app.ensure_utf8(argv);
    std::string from, to;
    unsigned int cluster_count, max_iter;
    app.add_option("-f,--from", from, "Source image path")->required();
    app.add_option("-t,--to", to, "Result image path")->required();
    app.add_option("-k,--clusters", cluster_count, "Number of clusters to use in k-means")->default_val(8);
    app.add_option("-i,--max_iter", max_iter, "Maximum number of k-means iterations.")->default_val(1000);
    CLI11_PARSE(app, argc, argv);
    std::cout << "Cluster Count: " << cluster_count << std::endl;

    // Load image
    unsigned char* image;
    unsigned int w, h;
    std::cout << "Reading " << from << "...\n";
    lodepng_decode_file(&image, &w, &h, from.c_str(), LCT_RGBA, 8);

    auto options = QuantizeOptions{
        .image = image,
        .image_len = w * h,
        .cluster_count = cluster_count,
        .iteration_limit = max_iter
    };

    // Process
    std::cout << "Clustering is starting...\n";
    auto start = std::chrono::high_resolution_clock::now();
    auto iterations = kmeans(options);
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Finished clustering " << w * h << " pixels (" << iterations << " iterations) in "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
        << " milliseconds. Storing image...\n";

    // Store output
    lodepng_encode_file(to.c_str(), image, w, h, LCT_RGBA, 8);
    std::cout << "Finished. Saved result to: " << to << std::endl;

}

static unsigned int kmeans(const QuantizeOptions& options)
{
    srand(static_cast<unsigned int>(time(0)));
    auto centroids = new float* [options.cluster_count];
    auto prev_centroids = new float* [options.cluster_count];
    init_centroids(centroids, options);
    auto clusters = new std::vector<unsigned char*>[options.cluster_count];
    init_clusters(clusters, options);
    bool converged = false;
    int last_iteration = 0;
    for (unsigned int iteration = 0; iteration < options.iteration_limit; iteration++)
    {
        last_iteration++;
        init_clusters(clusters, options);
        for (unsigned int point_idx = 0; point_idx < options.image_len; point_idx++)
        {
            float min_distance = -1;
            int min_idx = -1;
            auto point = options.image + 4 * point_idx;
            for (unsigned int centroid_idx = 0; centroid_idx < options.cluster_count; centroid_idx++)
            {
                auto distance = distance_from_centroid(centroids[centroid_idx], point);
                if (min_idx < 0 || distance < min_distance)
                {
                    min_distance = distance;
                    min_idx = centroid_idx;
                }
            }
            clusters[min_idx].push_back(point);
        }
        bool anyChanged = update_centroids(centroids, clusters, options);
        if (!anyChanged)
        {
            converged = true;
            break;
        }
    }
    if (!converged)
    {
        throw new std::runtime_error("Did not converge");
    }
    for (unsigned int i = 0; i < options.cluster_count; i++)
    {
        for (int j = 0; j < clusters[i].size(); j++)
        {
            clusters[i][j][0] = static_cast<unsigned char>(centroids[i][0]);
            clusters[i][j][1] = static_cast<unsigned char>(centroids[i][1]);
            clusters[i][j][2] = static_cast<unsigned char>(centroids[i][2]);
        }
    }
    return last_iteration;
}

static void init_centroids(float** centroids, const QuantizeOptions& options)
{
    for (unsigned int i = 0; i < options.cluster_count; i++)
    {
        auto randIndex = static_cast<int>(4.0f * (1.0f * std::rand() / RAND_MAX) * (options.image_len - 1));
        centroids[i] = new float[options.cluster_count] {
            static_cast<float>(options.image[randIndex]),
                static_cast<float>(options.image[randIndex + 1]),
                static_cast<float>(options.image[randIndex + 2])
            };
    }
}

static void init_clusters(std::vector<unsigned char*>* clusters, const QuantizeOptions& options)
{
    for (unsigned int i = 0; i < options.cluster_count; i++)
    {
        clusters[i].clear();
    }
}

static bool update_centroids(float** centroids, const std::vector<unsigned char*>* clusters, const QuantizeOptions& options)
{
    const float threshold = 0.01f;
    bool anyChanged = false;
    for (unsigned int i = 0; i < options.cluster_count; i++)
    {
        auto old_r = centroids[i][0];
        auto old_g = centroids[i][1];
        auto old_b = centroids[i][2];

        float new_r = 0;
        float new_g = 0;
        float new_b = 0;

        for (int j = 0; j < clusters[i].size(); j++)
        {
            new_r += clusters[i][j][0];
            new_g += clusters[i][j][1];
            new_b += clusters[i][j][2];
        }
        if (clusters[i].size() > 0)
        {
            new_r /= clusters[i].size();
            new_g /= clusters[i].size();
            new_b /= clusters[i].size();
        }
        centroids[i][0] = new_r;
        centroids[i][1] = new_g;
        centroids[i][2] = new_b;

        if (anyChanged || abs(((new_r - old_r) / old_r)) > threshold || abs(((new_g - old_g) / old_g)) > threshold || abs(((new_b - old_b) / old_b)) > threshold)
        {
            anyChanged = true;
        }
    }
    return anyChanged;
}

static float distance_from_centroid(const float* centroid, const unsigned char* point)
{
    float r = *point;
    float g = *(point + 1);
    float b = *(point + 2);

    return 
        (centroid[0] - r) * (centroid[0] - r) +
        (centroid[1] - g) * (centroid[1] - g) +
        (centroid[2] - b) * (centroid[2] - b);
}
